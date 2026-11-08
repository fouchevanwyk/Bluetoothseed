// Bluetooth side: pairing, advertising and sending media keys.
#pragma once
#include <Arduino.h>
#include <bluefruit.h>   // HID_USAGE_CONSUMER_* key codes

void bleBegin();       // start BLE, the HID service and advertising
bool bleConnected();

// Press and release one media key. Returns true if the phone got it.
// log = false keeps auto-repeat from flooding the serial log.
bool sendKey(uint16_t key, bool log = true);

// Call every loop: resends a key release that didn't get through and
// asks the phone for a battery-friendly connection interval.
void blePoll();

void bleLogStatus();   // one log line: phone, encryption, interval
