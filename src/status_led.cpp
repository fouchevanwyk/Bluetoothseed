#include "status_led.h"

// The XIAO's RGB LED is active-low (LOW = on).
static const uint8_t LED_PINS[] = { LED_RED, LED_GREEN, LED_BLUE };
static bool     ledLit   = false;
static uint32_t ledOffAt = 0;

void ledBegin() {
  for (uint8_t p : LED_PINS) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); }
}

void ledAllOff() {
  for (uint8_t p : LED_PINS) digitalWrite(p, HIGH);
  ledLit = false;
}

void ledFlash(uint8_t pin, uint32_t ms) {
  ledAllOff();
  digitalWrite(pin, LOW);
  ledLit   = true;
  ledOffAt = millis() + ms;
}

// Non-blocking: shows connection changes, blinks while waiting for the
// phone, and switches a flash off when its time is up.
void updateLed(bool connected) {
  static bool     wasConnected = false;
  static uint32_t lastBlink    = 0;
  uint32_t now = millis();

  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) ledFlash(LED_BLUE, 1000);
    else           ledFlash(LED_RED, 500);
  } else if (!connected && !ledLit && now - lastBlink >= 2000) {
    lastBlink = now;
    ledFlash(LED_BLUE, 30);
  }

  if (ledLit && (int32_t)(now - ledOffAt) >= 0) ledAllOff();
}
