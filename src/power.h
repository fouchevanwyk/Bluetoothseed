// Sleep when idle. After SLEEP_AFTER_MS with no phone and no button use
// the chip powers down (System OFF, a few uA). A button press, the reset
// button or plugging in USB wakes it; it restarts and the phone
// reconnects. It never sleeps while USB is connected to a computer, so
// the serial monitor and flashing keep working.
#pragma once

void noteActivity();                // a button changed: restart the countdown
void sleepIfIdle(bool connected);   // call every loop
