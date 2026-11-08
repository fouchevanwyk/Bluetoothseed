// RGB status LED. One colour at a time:
//   green flash = key sent          red flash  = key not sent
//   blue 1 s    = connected         red 0.5 s  = disconnected
//   short blue blink every 2 s      = waiting for the phone
//   two red blinks                  = going to sleep
#pragma once
#include <Arduino.h>

void ledBegin();
void ledAllOff();
void ledFlash(uint8_t pin, uint32_t ms);   // pin: LED_RED, LED_GREEN, LED_BLUE
void updateLed(bool connected);            // call every loop
