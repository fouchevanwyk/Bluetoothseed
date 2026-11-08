#include "watchdog.h"
#include <bluefruit.h>   // TinyUSBDevice
#include "config.h"

// Why the watchdog is off while plugged into a computer:
// Once started, the nRF52 watchdog cannot be stopped by software and it
// keeps running through a soft reset. Flashing does a soft reset into the
// bootloader, which never feeds the watchdog, so a running watchdog would
// reset the board halfway through an upload.
//
// So it is only armed when no computer is attached (battery or power
// bank). If a computer is plugged in while it is running, it is no longer
// fed: it resets the board within WDT_TIMEOUT_MS, which also switches the
// watchdog off, and it stays off while the computer is attached.
//
// System OFF sleep stops the watchdog; it is armed again after waking.

// Wait this long after boot before arming, so a computer has time to
// enumerate the USB port first.
static const uint32_t ARM_AFTER_MS = 3000;

static bool running() {
  return NRF_WDT->RUNSTATUS & WDT_RUNSTATUS_RUNSTATUS_Msk;
}

static void feed() {
  NRF_WDT->RR[0] = WDT_RR_RR_Reload;
}

static void arm() {
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run  << WDT_CONFIG_SLEEP_Pos)    // count while idle
                  | (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);    // not in debugger
  NRF_WDT->CRV  = WDT_TIMEOUT_MS * 32768 / 1000;   // 32.768 kHz ticks
  NRF_WDT->RREN = WDT_RREN_RR0_Msk;
  NRF_WDT->TASKS_START = 1;
  LOG("[WDT] armed, %lu s timeout\r\n", (unsigned long)(WDT_TIMEOUT_MS / 1000));
}

void watchdogBegin() {
  // After a soft reset the watchdog may still be running: feed it now so
  // setup() has the full timeout to finish.
  if (running()) feed();
}

void watchdogPoll() {
  static bool told = false;
  bool computer = TinyUSBDevice.mounted();

  if (running()) {
    if (!computer) { feed(); return; }
    if (!told) {
      told = true;
      LOG("[WDT] computer attached - restarting in %lu s to switch the "
          "watchdog off for flashing\r\n", (unsigned long)(WDT_TIMEOUT_MS / 1000));
    }
    return;   // not fed: the watchdog resets the board
  }

  if (!computer && millis() >= ARM_AFTER_MS) arm();
}

bool watchdogArmed() {
  return running();
}

const char *lastResetReason() {
  uint32_t r = readResetReason();   // captured by the core at boot
  if (r & POWER_RESETREAS_DOG_Msk)      return "watchdog";
  if (r & POWER_RESETREAS_LOCKUP_Msk)   return "CPU lockup";
  if (r & POWER_RESETREAS_SREQ_Msk)     return "software (crash or flashing)";
  if (r & POWER_RESETREAS_RESETPIN_Msk) return "reset button";
  if (r & POWER_RESETREAS_OFF_Msk)      return "woke from sleep (button)";
  if (r & POWER_RESETREAS_VBUS_Msk)     return "woke from sleep (USB)";
  return "power on";
}
