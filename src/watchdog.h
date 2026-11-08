// Hardware watchdog: if loop() stops running for WDT_TIMEOUT_MS, the
// chip resets itself and starts again. Only armed while the remote is
// not plugged into a computer (see watchdog.cpp for why).
#pragma once

void watchdogBegin();               // call first in setup()
void watchdogPoll();                // call every loop: feeds or arms it
bool watchdogArmed();
const char *lastResetReason();      // why the chip last restarted
