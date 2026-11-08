// 2 x AAA battery level: measured on D0 and shown on the phone as a
// percentage (standard Bluetooth Battery Service).
#pragma once
#include <Arduino.h>

void batteryBegin();       // add the Battery Service; called from bleBegin()
void batteryPoll();        // call every loop: measures once a minute
void batteryLogStatus();   // one log line with the last reading
