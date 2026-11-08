#include "power.h"
#include <bluefruit.h>   // sd_power_system_off, TinyUSBDevice
#include "config.h"
#include "status_led.h"

static uint32_t lastActivity = 0;   // millis() of last button change / connection

void noteActivity() {
  lastActivity = millis();
}

void sleepIfIdle(bool connected) {
  uint32_t now = millis();
  if (connected || TinyUSBDevice.mounted()) {
    lastActivity = now;       // full timeout again once these end
    return;
  }
  if (now - lastActivity < SLEEP_AFTER_MS) return;

  // Two red blinks: going to sleep.
  for (int i = 0; i < 2; i++) {
    ledFlash(LED_RED, 100); delay(100);
    ledAllOff();            delay(150);
  }
  // LED pins stay driven HIGH (off) through System OFF.

  // Wake on any button that is up now. A pin stuck on GND is skipped,
  // otherwise it would wake the chip straight away.
  for (uint8_t pin : BUTTON_PINS)
    if (digitalRead(pin) == HIGH)
      nrf_gpio_cfg_sense_input(g_ADigitalPinMap[pin], NRF_GPIO_PIN_PULLUP,
                               NRF_GPIO_PIN_SENSE_LOW);

  sd_power_system_off();      // does not return; waking restarts setup()
}
