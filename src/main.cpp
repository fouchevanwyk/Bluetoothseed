// XIAO Remote: a Bluetooth media remote for the Seeed XIAO nRF52840.
// Five buttons to GND send play/pause, volume and track keys to a phone.
//
//   config.h        pins, timings, debug switch (start here)
//   buttons.cpp     debounce, volume repeat, wiring check
//   hid_remote.cpp  Bluetooth pairing, advertising, sending keys
//   status_led.cpp  RGB LED patterns
//   power.cpp       sleep when idle
//   watchdog.cpp    restart automatically if the program freezes
#include "config.h"
#include "buttons.h"
#include "hid_remote.h"
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
  flushRelease();
  buttonsPoll();

  bool connected = bleConnected();
  updateLed(connected);
  sleepIfIdle(connected);

  delay(POLL_MS);   // debouncing is done in buttons.cpp
}
