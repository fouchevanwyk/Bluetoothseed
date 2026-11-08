// All settings for the remote in one place: pins, timings, debug switch.
#pragma once
#include <Arduino.h>

// ---- Device -----------------------------------------------------------
#define DEVICE_NAME   "XIAO Remote"   // name shown on the phone
#define MANUFACTURER  "DIY"
constexpr int8_t TX_POWER_DBM = 4;    // radio power (-40..+8)

// Connection interval the remote asks the phone for, in 1.25 ms units.
// 24-48 = 30-60 ms: still instant for buttons, much easier on the battery
// than the 11-15 ms phones often start with.
constexpr uint16_t CONN_INTERVAL_MIN = 24;
constexpr uint16_t CONN_INTERVAL_MAX = 48;

// ---- The button -------------------------------------------------------
// One button from D1 to GND (internal pull-up) does everything:
//   tap              play / pause
//   double tap       next track
//   triple tap       previous track
//   hold             volume up   (repeats while held)
//   tap, then hold   volume down (repeats while held)
constexpr uint8_t BTN = D1;
constexpr uint8_t BUTTON_PINS[] = { BTN };   // pins that wake it from sleep

// ---- Battery ----------------------------------------------------------
// 2 x AAA measured on D0 through a 1M/1M divider with 100 nF to GND
// (after the on/off switch, before the diode).
constexpr uint8_t PIN_BATT = D0;
// Which cells are fitted, for the percentage shown on the phone:
// false = alkaline (normal throw-away AAA), true = NiMH rechargeable.
constexpr bool BATTERY_NIMH = false;
constexpr uint8_t  LOW_BATT_PCT  = 15;           // red flash each minute below this
constexpr uint32_t BATT_EVERY_MS = 60UL * 1000;  // how often it is measured

// ---- Timings (ms) -----------------------------------------------------
constexpr uint32_t POLL_MS         = 10;    // how often the button is read
constexpr uint32_t DEBOUNCE_MS     = 25;    // pin must be stable this long
constexpr uint32_t KEY_HOLD_MS     = 20;    // gap between key press and release

constexpr uint32_t TAP_GAP_MS      = 350;   // pause that ends a tap sequence
constexpr uint32_t HOLD_MS         = 500;   // pressed this long = hold (volume)

// Volume repeat while held. Repeats stop after REPEAT_LIMIT_MS so a
// button or wire stuck on GND can't max out the volume; release and hold
// again to keep going.
constexpr uint32_t REPEAT_EVERY_MS = 150;   // one volume step per interval
constexpr uint32_t REPEAT_LIMIT_MS = 4000;  // stop repeating after this
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
