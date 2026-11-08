#include "hid_remote.h"
#include "config.h"
#include "status_led.h"
#include "battery.h"

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

static void flushRelease() {
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

// ---- Connection interval --------------------------------------------
// Phones usually connect with an 11-15 ms interval. A few seconds after
// connecting (once pairing and service discovery are done) ask for the
// slower CONN_INTERVAL_MIN..MAX instead: the radio wakes 3-4x less often.
static void tuneInterval() {
  static bool     wasConnected = false, asked = false;
  static uint32_t connectedAt  = 0;
  static uint16_t shown        = 0;

  bool connected = bleConnected();
  if (connected != wasConnected) {
    wasConnected = connected;
    connectedAt  = millis();
    asked = false;
    shown = 0;
  }
  if (!connected) return;

  BLEConnection *conn = Bluefruit.Connection(Bluefruit.connHandle());
  if (!conn) return;
  uint16_t iv = conn->getConnectionInterval();   // 1.25 ms units
  if (iv != shown) {
    shown = iv;
    LOG("[BLE] connection interval %u.%02u ms\r\n", iv * 125 / 100, iv * 125 % 100);
  }
  if (!asked && conn->secured() && millis() - connectedAt >= 5000 &&
      (iv < CONN_INTERVAL_MIN || iv > CONN_INTERVAL_MAX)) {
    asked = true;
    // NULL = use the preferred values set with setConnInterval().
    uint32_t err = sd_ble_gap_conn_param_update(conn->handle(), NULL);
    LOG("[BLE] asking the phone for %u-%u ms to save battery%s\r\n",
        CONN_INTERVAL_MIN * 5 / 4, CONN_INTERVAL_MAX * 5 / 4,
        err ? " - request failed" : "");
  }
}

void bleLogStatus() {
  BLEConnection *conn = Bluefruit.Connection(Bluefruit.connHandle());
  if (!bleConnected() || !conn) { LOG("link: advertising, waiting for the phone\r\n"); return; }
  char peer[32] = { 0 };
  conn->getPeerName(peer, sizeof(peer));
  uint16_t iv = conn->getConnectionInterval();
  LOG("link: \"%s\" | %s | interval %u.%02u ms\r\n", peer,
      conn->secured() ? "encrypted" : "NOT encrypted yet", iv * 125 / 100, iv * 125 % 100);
}

void blePoll() {
  flushRelease();
  tuneInterval();
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
  batteryBegin();
  Bluefruit.setAppearance(BLE_APPEARANCE_GENERIC_HID);
  // The interval we'd like; the phone picks the first one itself, so
  // tuneInterval() asks again once connected.
  Bluefruit.Periph.setConnInterval(CONN_INTERVAL_MIN, CONN_INTERVAL_MAX);

  startAdv();
}
