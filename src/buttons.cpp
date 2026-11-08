#include "buttons.h"
#include "config.h"
#include "hid_remote.h"
#include "battery.h"
#include "power.h"
#include "watchdog.h"

struct Button {
  uint8_t     pin;
  const char *name;
  bool        raw;        // last raw reading (true = pressed)
  bool        stable;     // debounced state
  uint32_t    changedAt;  // millis() when raw last changed
  uint32_t    stableAt;   // millis() of the last debounced change
  uint8_t     edges;      // raw transitions since last debounced change
};

static Button button = { BTN, "BUTTON" };

#if DEBUG_LOG
// Wiring check: the unused D-pins are watched too, so a wire on the
// wrong pin shows up in the log instead of doing nothing. D0 is left out:
// it measures the battery, and a pull-up there would spoil the reading.
static Button sparePins[] = {
  { D2, "D2 (unused)" }, { D3, "D3 (unused)" }, { D4, "D4 (unused)" },
  { D5, "D5 (unused)" }, { D6, "D6 (unused)" }, { D7, "D7 (unused)" },
  { D8, "D8 (unused)" }, { D9, "D9 (unused)" }, { D10, "D10 (unused)" },
};
#endif

// Debounced read: a change only counts once the pin has held the new
// level for DEBOUNCE_MS, so contact bounce and glitches are ignored.
// Returns true while the button is pressed.
static bool readButton(Button &b, uint32_t now) {
  bool r = digitalRead(b.pin) == LOW;
  if (r != b.raw) { b.raw = r; b.changedAt = now; b.edges++; }
  if (b.stable != b.raw && now - b.changedAt >= DEBOUNCE_MS) {
    b.stable = b.raw;
    noteActivity();
    // How long it was in the previous state: handy for tuning the
    // TAP_GAP_MS / HOLD_MS timings.
    LOG("[BTN] %s %s (was %s %lu ms)", b.name, b.stable ? "down" : "up",
        b.stable ? "up" : "down", (unsigned long)(now - b.stableAt));
    if (b.edges > 1) LOG(" (%u glitch edges ignored)", b.edges - 1);
    LOG("\r\n");
    b.stableAt = now;
    b.edges = 0;
  }
  return b.stable;
}

// ---- Gestures -----------------------------------------------------------
// Runs on the debounced state, so a scratchy contact can't end a hold
// early or turn one tap into two.
enum class State : uint8_t {
  Idle,       // nothing going on
  Pressed,    // down, not yet long enough to be a hold
  Released,   // tap(s) done, waiting TAP_GAP_MS for another tap
  Holding,    // held: stepping the volume
};

static State    state = State::Idle;
static uint8_t  taps;                   // taps in the current sequence
static uint32_t pressedAt, releasedAt;
static uint32_t holdStart, lastRepeat, lastNote;
static uint16_t holdKey, steps;

static void tapsDone() {
  static const char *const NAMES[] = { "single tap", "double tap", "triple tap" };
  static const uint16_t KEYS[] = { HID_USAGE_CONSUMER_PLAY_PAUSE,
                                   HID_USAGE_CONSUMER_SCAN_NEXT,
                                   HID_USAGE_CONSUMER_SCAN_PREVIOUS };
  LOG("[GESTURE] %s\r\n", NAMES[taps - 1]);
  sendKey(KEYS[taps - 1]);
  taps  = 0;
  state = State::Idle;
}

static void startHold(uint32_t now) {
  // A hold straight away is volume up; a tap (or more) just before it
  // makes it volume down.
  holdKey = taps ? HID_USAGE_CONSUMER_VOLUME_DECREMENT
                 : HID_USAGE_CONSUMER_VOLUME_INCREMENT;
  LOG("[GESTURE] %s\r\n", taps ? "tap + hold" : "hold");
  taps  = 0;
  state = State::Holding;
  holdStart = lastRepeat = lastNote = now;
  steps = sendKey(holdKey) ? 1 : 0;
}

static void handleGestures(bool down, uint32_t now) {
  switch (state) {
    case State::Idle:
      if (down) { state = State::Pressed; pressedAt = now; }
      break;

    case State::Pressed:
      if (!down) {                                  // short press = a tap
        taps++;
        releasedAt = now;
        // Nothing comes after three taps, so act now instead of waiting.
        if (taps >= 3) tapsDone();
        else           state = State::Released;
      } else if (now - pressedAt >= HOLD_MS) {
        startHold(now);
      }
      break;

    case State::Released:
      if (down) { state = State::Pressed; pressedAt = now; }   // another tap
      else if (now - releasedAt >= TAP_GAP_MS) tapsDone();
      break;

    case State::Holding:
      if (!down) {
        uint32_t held = now - holdStart;
        LOG("[GESTURE] hold ended: %lu ms, %u volume step(s)%s\r\n",
            (unsigned long)held, steps,
            held >= REPEAT_LIMIT_MS ? " (repeat stopped at limit)" : "");
        state = State::Idle;
        break;
      }
      if (now - holdStart < REPEAT_LIMIT_MS &&
          now - lastRepeat >= REPEAT_EVERY_MS) {
        lastRepeat = now;
        if (sendKey(holdKey, false)) steps++;   // repeats summarised at the end
      }
      // A hold this long is suspicious: likely a wire stuck on GND.
      if (now - holdStart >= STUCK_WARN_MS && now - lastNote >= STUCK_WARN_MS) {
        lastNote = now;
        LOG("[GESTURE] still held after %lu s - button stuck on GND?\r\n",
            (unsigned long)((now - holdStart) / 1000));
      }
      break;
  }
}

// Print a status banner whenever a serial terminal attaches, so the
// current state is visible even though boot messages were missed.
static void logBannerOnSerialOpen() {
#if DEBUG_LOG
  static bool wasOpen = false;
  bool open = Serial;   // true while a terminal holds the port (DTR)
  if (open && !wasOpen) {
    LOG("\r\n=== " DEVICE_NAME " (one button) | %s | up %lu s ===\r\n",
        bleConnected() ? "CONNECTED" : "advertising",
        (unsigned long)(millis() / 1000));
    LOG("last reset: %s | watchdog: %s\r\n", lastResetReason(),
        watchdogArmed() ? "on" : "off (computer attached)");
    bleLogStatus();
    batteryLogStatus();
    // Raw pin levels: LOW means the pin is connected to GND right now.
    LOG("pins: D1(BUTTON)=%s |", digitalRead(BTN) == LOW ? "LOW" : "high");
    for (Button &b : sparePins)
      if (digitalRead(b.pin) == LOW) LOG(" %s=LOW", b.name);
    LOG("\r\n");
  }
  wasOpen = open;
#endif
}

void buttonsBegin() {
  for (uint8_t pin : BUTTON_PINS) pinMode(pin, INPUT_PULLUP);
#if DEBUG_LOG
  for (Button &b : sparePins) pinMode(b.pin, INPUT_PULLUP);
#endif
}

void buttonsPoll() {
  uint32_t now = millis();
  logBannerOnSerialOpen();
  handleGestures(readButton(button, now), now);
#if DEBUG_LOG
  for (Button &b : sparePins) readButton(b, now);
#endif
}
