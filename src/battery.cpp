#include "battery.h"
#include <bluefruit.h>
#include "config.h"
#include "hid_remote.h"
#include "status_led.h"

static BLEBas blebas;

// The 100 nF capacitor on D0 charges through the divider (500 kOhm), so
// it needs about 250 ms after power-on before the reading is right.
static const uint32_t FIRST_READ_MS = 500;

// Below this the chip couldn't run from the pack, so it must be powered
// by USB (battery switched off or not fitted).
static const uint16_t NO_BATTERY_MV = 1500;

static bool     measured = false;
static uint16_t packMv   = 0;    // last reading
static uint8_t  percent  = 0;

// Pack voltage -> charge left, for two cells under the remote's light load.
struct Point { uint16_t mv; uint8_t pct; };
static const Point ALKALINE[] = {   // slopes down steadily as it is used
  { 3000, 100 }, { 2800, 80 }, { 2600, 60 }, { 2450, 40 },
  { 2300, 20 }, { 2200, 10 }, { 2100, 0 },
};
static const Point NIMH[] = {       // long flat middle, then drops fast
  { 2700, 100 }, { 2600, 90 }, { 2500, 70 }, { 2440, 50 },
  { 2400, 35 }, { 2340, 20 }, { 2260, 10 }, { 2100, 0 },
};

static uint8_t toPercent(uint16_t mv) {
  const Point *c = BATTERY_NIMH ? NIMH : ALKALINE;
  size_t n = BATTERY_NIMH ? sizeof(NIMH) / sizeof(NIMH[0])
                          : sizeof(ALKALINE) / sizeof(ALKALINE[0]);
  if (mv >= c[0].mv) return 100;
  for (size_t i = 1; i < n; i++) {
    if (mv >= c[i].mv)   // between point i and the one above: interpolate
      return c[i].pct + (uint32_t)(mv - c[i].mv) * (c[i - 1].pct - c[i].pct)
                        / (c[i - 1].mv - c[i].mv);
  }
  return 0;
}

static uint16_t readPackMv() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(PIN_BATT);
  // 12-bit reading of 0..3.6 V, then x2 to undo the 1M/1M divider.
  return (sum / 8) * 3600 * 2 / 4095;
}

static bool onBattery() {
  return packMv >= NO_BATTERY_MV;
}

void batteryLogStatus() {
  if (!measured)
    LOG("battery: not measured yet\r\n");
  else if (!onBattery())
    LOG("battery: none (D0 reads %u mV) - running on USB\r\n", packMv);
  else
    LOG("battery: %u.%02u V = %u%% (%s)%s\r\n", packMv / 1000,
        (packMv % 1000) / 10, percent, BATTERY_NIMH ? "NiMH" : "alkaline",
        percent < LOW_BATT_PCT ? " LOW" : "");
}

void batteryBegin() {
  analogReadResolution(12);   // the core defaults to 10 bits
  blebas.begin();
  blebas.write(100);          // until the first real reading
}

void batteryPoll() {
  static uint32_t lastRead = 0;
  uint32_t now = millis();
  if (!measured ? now < FIRST_READ_MS : now - lastRead < BATT_EVERY_MS)
    return;
  lastRead = now;
  measured = true;

  packMv  = readPackMv();
  // No battery: report 100% so the phone doesn't show an empty remote.
  percent = onBattery() ? toPercent(packMv) : 100;
  LOG("[BAT] ");
  batteryLogStatus();

  blebas.write(percent);
  if (bleConnected()) blebas.notify(percent);   // phone sees it straight away
  if (onBattery() && percent < LOW_BATT_PCT) ledFlash(LED_RED, 30);
}
