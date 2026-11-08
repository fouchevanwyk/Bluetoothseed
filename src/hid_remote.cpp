#include "hid_remote.h"
#include "config.h"
#include "status_led.h"

static BLEDis bledis;

// Media keys only. A keyboard in the report map would make phones treat
// the remote as a hardware keyboard and hide the on-screen keyboard.
static const uint8_t RID_CONSUMER = 1;
static const uint8_t hidReportMap[] = {
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER))
};
static uint16_t hidInputLen[] = { 2 };   // one 16-bit consumer usage
static BLEHidGeneric blehid(1);          // one input report, no outputs

// ---- Sending keys -----------------------------------------------------
static const char *keyName(uint16_t key) {
  switch (key) {
    case HID_USAGE_CONSUMER_PLAY_PAUSE:       return "PLAY/PAUSE";
    case HID_USAGE_CONSUMER_SCAN_NEXT:        return "NEXT TRACK";
    case HID_USAGE_CONSUMER_SCAN_PREVIOUS:    return "PREV TRACK";
    case HID_USAGE_CONSUMER_VOLUME_INCREMENT: return "VOLUME UP";
    case HID_USAGE_CONSUMER_VOLUME_DECREMENT: return "VOLUME DOWN";
    default:                                  return "?";
  }
}

static bool sendConsumer(uint16_t usage) {
  return blehid.inputReport(RID_CONSUMER, &usage, sizeof(usage));
}

// Set when the phone saw a press but not yet its release. Retried every
// loop so a weak link can never leave a key held down on the phone.
static bool releasePending = false;

void flushRelease() {
  if (!releasePending) return;
  // On disconnect the phone releases all keys itself.
  if (!bleConnected() || sendConsumer(0)) releasePending = false;
}

bool sendKey(uint16_t key, bool log) {
  if (!bleConnected()) {
    ledFlash(LED_RED, 40);
    if (log) LOG("[KEY] %s dropped: not connected\r\n", keyName(key));
    return false;
  }
  bool ok = sendConsumer(key);
  if (ok) {                 // only a press the phone saw needs a release
    delay(KEY_HOLD_MS);
    releasePending = true;
    flushRelease();
    if (releasePending)
      LOG("[KEY] %s release not sent - retrying\r\n", keyName(key));
  }
  ledFlash(ok ? LED_GREEN : LED_RED, 40);
  if (log) LOG("[KEY] %s %s\r\n", keyName(key),
               ok ? "sent" : "FAILED (host not subscribed yet?)");
  return ok;
}

bool bleConnected() {
  return Bluefruit.connected();
}

// ---- Connection events (logged only) ----------------------------------
static void onConnect(uint16_t conn) {
  char peer[32] = { 0 };
  Bluefruit.Connection(conn)->getPeerName(peer, sizeof(peer));
  LOG("[BLE] connected to \"%s\"\r\n", peer);
}

static void onDisconnect(uint16_t conn, uint8_t reason) {
  (void) conn;
  LOG("[BLE] disconnected, reason 0x%02X - advertising again\r\n", reason);
}

static void onPairComplete(uint16_t conn, uint8_t status) {
  (void) conn;
  LOG("[BLE] pairing %s (status 0x%02X)\r\n",
      status == BLE_GAP_SEC_STATUS_SUCCESS ? "OK" : "FAILED", status);
}

static void onSecured(uint16_t conn) {
  (void) conn;
  LOG("[BLE] link encrypted - ready to send keys\r\n");
}

// ---- Setup ------------------------------------------------------------
static void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_GENERIC_HID);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);   // 20 ms fast, 152.5 ms slow
  Bluefruit.Advertising.setFastTimeout(30);     // fast for the first 30 s
  Bluefruit.Advertising.start(0);               // until connected or asleep
}

void bleBegin() {
  // The LED is driven by status_led.cpp; stop Bluefruit's own blinking.
  Bluefruit.autoConnLed(false);
  Bluefruit.begin();
  Bluefruit.setTxPower(TX_POWER_DBM);
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);
  Bluefruit.Security.setPairCompleteCallback(onPairComplete);
  Bluefruit.Security.setSecuredCallback(onSecured);

  bledis.setManufacturer(MANUFACTURER);
  bledis.setModel(DEVICE_NAME);
  bledis.begin();

  blehid.setReportLen(hidInputLen, NULL, NULL);
  blehid.setReportMap(hidReportMap, sizeof(hidReportMap));
  blehid.begin();
  Bluefruit.setAppearance(BLE_APPEARANCE_GENERIC_HID);
  // 11.25-15 ms connection interval for low key latency.
  Bluefruit.Periph.setConnInterval(9, 12);

  startAdv();
}
