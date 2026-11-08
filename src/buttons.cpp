#include "buttons.h"
#include "config.h"
#include "hid_remote.h"
#include "power.h"
#include "watchdog.h"

struct Button {
  uint8_t     pin;
  const char *name;
  bool        raw;        // last raw reading (true = pressed)
  bool        stable;     // debounced state
  uint32_t    changedAt;  // millis() when raw last changed
  uint8_t     edges;      // raw transitions since last debounced change
};

// One key per press, sent as soon as the press is detected.
struct TapButton {
  Button   btn;
  uint16_t key;
  bool     wasDown;
};

// Key on press, then auto-repeat while held.
struct VolumeButton {
  Button   btn;
  uint16_t key;
  bool     wasDown;
  uint32_t pressStart, lastRepeat;
  uint16_t steps;         // keys sent during the current hold
  uint32_t lastNote;      // last "still held" log line
};

static TapButton tapPlay = { { BTN_PLAY, "PLAY" }, HID_USAGE_CONSUMER_PLAY_PAUSE };
static TapButton tapNext = { { BTN_NEXT, "NEXT" }, HID_USAGE_CONSUMER_SCAN_NEXT };
static TapButton tapPrev = { { BTN_PREV, "PREV" }, HID_USAGE_CONSUMER_SCAN_PREVIOUS };
static VolumeButton volUp = { { BTN_VUP, "VOL+" }, HID_USAGE_CONSUMER_VOLUME_INCREMENT };
static VolumeButton volDn = { { BTN_VDN, "VOL-" }, HID_USAGE_CONSUMER_VOLUME_DECREMENT };

#if DEBUG_LOG
// Wiring check: the unused D-pins are watched too, so a wire on the
// wrong pin shows up in the log instead of doing nothing.
static Button sparePins[] = {
  { D0, "D0 (unused)" }, { D6, "D6 (unused)" }, { D7, "D7 (unused)" },
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
    LOG("[BTN] %s %s", b.name, b.stable ? "down" : "up");
    if (b.edges > 1) LOG(" (%u glitch edges ignored)", b.edges - 1);
    LOG("\r\n");
    b.edges = 0;
  }
  return b.stable;
}

static void handleTap(TapButton &t) {
  bool down = readButton(t.btn, millis());
  if (down && !t.wasDown) sendKey(t.key);
  t.wasDown = down;
}

static void handleVolume(VolumeButton &v) {
  uint32_t now = millis();
  bool down = readButton(v.btn, now);

  if (down && !v.wasDown) {                 // first press
    v.pressStart = v.lastRepeat = now;
    v.steps = sendKey(v.key) ? 1 : 0;
  } else if (down && now - v.pressStart > REPEAT_DELAY_MS &&
             now - v.pressStart < REPEAT_LIMIT_MS &&
             now - v.lastRepeat > REPEAT_EVERY_MS) {   // held -> repeat
    v.lastRepeat = now;
    if (sendKey(v.key, false)) v.steps++;   // repeats summarised on release
  } else if (!down && v.wasDown) {
    uint32_t held = now - v.pressStart;
    LOG("[VOL] %s held %lu ms, %u step(s) sent%s\r\n", v.btn.name,
        (unsigned long)held, v.steps,
        held >= REPEAT_LIMIT_MS ? " (repeat stopped at limit)" : "");
  }
  // A hold this long is suspicious: likely a wire stuck on GND.
  if (down && now - v.pressStart >= STUCK_WARN_MS &&
      now - v.lastNote >= STUCK_WARN_MS) {
    v.lastNote = now;
    LOG("[VOL] %s still held after %lu s (%u steps) - pin stuck on GND?\r\n",
        v.btn.name, (unsigned long)((now - v.pressStart) / 1000), v.steps);
  }
  v.wasDown = down;
}

// Print a status banner whenever a serial terminal attaches, so the
// current state is visible even though boot messages were missed.
static void logBannerOnSerialOpen() {
#if DEBUG_LOG
  static bool wasOpen = false;
  bool open = Serial;   // true while a terminal holds the port (DTR)
  if (open && !wasOpen) {
    LOG("\r\n=== " DEVICE_NAME " | %s | up %lu s ===\r\n",
        bleConnected() ? "CONNECTED" : "advertising",
        (unsigned long)(millis() / 1000));
    LOG("last reset: %s | watchdog: %s\r\n", lastResetReason(),
        watchdogArmed() ? "on" : "off (computer attached)");
    // Raw pin levels: LOW means the pin is connected to GND right now.
    LOG("pins: D1(PLAY)=%s D2(VOL+)=%s D3(VOL-)=%s D4(NEXT)=%s D5(PREV)=%s |",
        digitalRead(BTN_PLAY) == LOW ? "LOW" : "high",
        digitalRead(BTN_VUP)  == LOW ? "LOW" : "high",
        digitalRead(BTN_VDN)  == LOW ? "LOW" : "high",
        digitalRead(BTN_NEXT) == LOW ? "LOW" : "high",
        digitalRead(BTN_PREV) == LOW ? "LOW" : "high");
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
  logBannerOnSerialOpen();
  handleTap(tapPlay);
  handleTap(tapNext);
  handleTap(tapPrev);
  handleVolume(volUp);
  handleVolume(volDn);
#if DEBUG_LOG
  for (Button &b : sparePins) readButton(b, millis());
#endif
}
