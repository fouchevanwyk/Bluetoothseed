// All settings for the remote in one place: pins, timings, debug switch.
#pragma once
#include <Arduino.h>

// ---- Device -----------------------------------------------------------
#define DEVICE_NAME   "XIAO Remote"   // name shown on the phone
#define MANUFACTURER  "DIY"
constexpr int8_t TX_POWER_DBM = 4;    // radio power (-40..+8)

// ---- Buttons ----------------------------------------------------------
// Every button connects its pin to GND (internal pull-ups).
constexpr uint8_t BTN_PLAY = D1;  // play/pause
constexpr uint8_t BTN_VUP  = D2;  // volume up   (repeats while held)
constexpr uint8_t BTN_VDN  = D3;  // volume down (repeats while held)
constexpr uint8_t BTN_NEXT = D4;  // next track
constexpr uint8_t BTN_PREV = D5;  // previous track

constexpr uint8_t BUTTON_PINS[] = { BTN_PLAY, BTN_VUP, BTN_VDN, BTN_NEXT, BTN_PREV };

// ---- Timings (ms) -----------------------------------------------------
constexpr uint32_t POLL_MS         = 10;    // how often buttons are read
constexpr uint32_t DEBOUNCE_MS     = 25;    // pin must be stable this long
constexpr uint32_t KEY_HOLD_MS     = 20;    // gap between key press and release

// Volume auto-repeat. Repeats stop after REPEAT_LIMIT_MS so a wire stuck
// on GND can't keep forcing the volume up; release to repeat again.
constexpr uint32_t REPEAT_DELAY_MS = 400;   // hold this long before repeating
constexpr uint32_t REPEAT_EVERY_MS = 120;   // then one step per interval
constexpr uint32_t REPEAT_LIMIT_MS = 3000;  // stop repeating after this
constexpr uint32_t STUCK_WARN_MS   = 5000;  // log "pin stuck?" after this

// Power down after this long with no phone connected and no button use.
constexpr uint32_t SLEEP_AFTER_MS  = 5UL * 60 * 1000;   // 5 minutes

// Restart automatically if the program freezes for this long. The
// longest normal pause in loop() is about 2 s.
constexpr uint32_t WDT_TIMEOUT_MS  = 5000;

// ---- Debug log --------------------------------------------------------
// Log over USB serial. Writes are skipped when no terminal is attached,
// so this is safe on battery. Set to 0 (or build with -DDEBUG_LOG=0) to
// compile logging and the wiring check out.
#ifndef DEBUG_LOG
#define DEBUG_LOG 1
#endif

#if DEBUG_LOG
  #define LOG(...) Serial.printf(__VA_ARGS__)
#else
  // Never runs and compiles to nothing, but keeps the arguments "used"
  // so turning logging off doesn't cause unused-variable warnings.
  #define LOG(...) do { if (0) Serial.printf(__VA_ARGS__); } while (0)
#endif
