// XIAO Remote: a Bluetooth media remote for the Seeed XIAO nRF52840.
// One button to GND sends play/pause, track and volume keys to a phone
// (tap, double tap, triple tap, hold, tap + hold - see config.h).
//
//   config.h        pins, timings, debug switch (start here)
//   buttons.cpp     debounce, tap / hold gestures, wiring check
//   hid_remote.cpp  Bluetooth pairing, advertising, sending keys
//   battery.cpp     2 x AAA level shown on the phone
//   status_led.cpp  RGB LED patterns
//   power.cpp       sleep when idle
//   watchdog.cpp    restart automatically if the program freezes
#include "config.h"
#include "buttons.h"
#include "hid_remote.h"
#include "battery.h"
#include "status_led.h"
#include "power.h"
#include "watchdog.h"

void setup() {
  watchdogBegin();
  Serial.begin(115200);
  ledBegin();
  buttonsBegin();
  bleBegin();
}

void loop() {
  watchdogPoll();
  blePoll();
  buttonsPoll();
  batteryPoll();

  bool connected = bleConnected();
  updateLed(connected);
  sleepIfIdle(connected);

  delay(POLL_MS);   // debouncing is done in buttons.cpp
}
