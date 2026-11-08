// Task 4 Lv2: throughput test, XIAO nRF52840 BLE peripheral, Nordic UART Service (NUS)
// Board: Seeed nRF52 Boards (non-mbed) > Seeed XIAO nRF52840 (Sense)
//
// USB serial commands (115200, one line each, sent by measure/throughput.py):
//   CFG <len> <ci_ms>   packet length in bytes (20..244) and requested connection interval in ms
//   START               reset counters, send packets as fast as the link allows
//   STOP                stop sending, print totals
//   STAT                print the connection's actual MTU, interval and data length
//
// Packet (ASCII): "<seq>,<millis>," + 'x' padding + "\n", exactly <len> bytes.
// The receiver reads lines; a gap in seq means a lost packet.
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

BLEUart bleuart;

static const uint16_t PKT_MAX = 244;     // MTU 247 - 3 bytes ATT header

uint16_t conn_hdl = BLE_CONN_HANDLE_INVALID;
uint16_t pkt_len = 100;
uint16_t ci_units = 12;                  // connection interval, unit 1.25 ms (12 = 15 ms)
bool running = false;
uint32_t seq = 0, tx_pkts = 0, tx_bytes = 0, t_start = 0;

uint8_t pend[PKT_MAX];                   // packet being written; kept until the FIFO took all of it
uint16_t pend_len = 0, pend_off = 0;

void connect_callback(uint16_t hdl) {
  conn_hdl = hdl;
  // No PHY / data length / MTU / interval requests here: sent right after connecting they
  // collided with the central's security + service discovery and it never enabled notifications.
  // The interval is requested later by the CFG command.
}

void disconnect_callback(uint16_t hdl, uint8_t reason) {
  (void)hdl; (void)reason;
  conn_hdl = BLE_CONN_HANDLE_INVALID;
}

void setup() {
  Serial.begin(115200);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);   // must be called before begin()
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("XIAO1");
  Bluefruit.Periph.setConnInterval(ci_units, ci_units);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  bleuart.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void build_packet() {
  int n = snprintf((char*)pend, PKT_MAX, "%lu,%lu,", (unsigned long)seq, (unsigned long)millis());
  for (int i = n; i < pkt_len - 1; i++) pend[i] = 'x';
  pend[pkt_len - 1] = '\n';
  pend_len = pkt_len;
  pend_off = 0;
}

void handle_line(String line) {
  line.trim();
  if (line.startsWith("CFG")) {
    // "CFG <len> <ci_ms>"; sscanf can't read floats in this core's newlib-nano
    int sp = line.indexOf(' ', 4);
    int len = line.substring(4, sp).toInt();
    float ci_ms = line.substring(sp + 1).toFloat();
    if (sp > 0 && len > 0 && ci_ms > 0) {
      pkt_len = constrain(len, 20, PKT_MAX);
      ci_units = (uint16_t)(ci_ms / 1.25f + 0.5f);
      Bluefruit.Periph.setConnInterval(ci_units, ci_units);
      if (conn_hdl != BLE_CONN_HANDLE_INVALID) {
        Bluefruit.Connection(conn_hdl)->requestConnectionParameter(ci_units);
      }
      pend_len = 0;
      Serial.printf("OK CFG len=%u ci_units=%u\n", pkt_len, ci_units);
    } else {
      Serial.println("ERR CFG");
    }
  } else if (line == "START") {
    seq = tx_pkts = tx_bytes = 0;
    pend_len = 0;
    t_start = millis();
    running = true;
    Serial.println("OK START");
  } else if (line == "STOP") {
    running = false;
    Serial.printf("DONE pkts=%lu bytes=%lu ms=%lu\n",
                  (unsigned long)tx_pkts, (unsigned long)tx_bytes, (unsigned long)(millis() - t_start));
  } else if (line == "STAT") {
    if (conn_hdl == BLE_CONN_HANDLE_INVALID) {
      Serial.println("STAT disconnected");
    } else {
      BLEConnection* c = Bluefruit.Connection(conn_hdl);
      Serial.printf("STAT mtu=%u ci_units=%u dl=%u\n", c->getMtu(), c->getConnectionInterval(), c->getDataLength());
    }
  }
}

void loop() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { handle_line(line); line = ""; }
    else if (c != '\r') line += c;
  }

  if (!running || !bleuart.notifyEnabled()) return;

  for (int i = 0; i < 8; i++) {          // small burst per loop pass
    if (pend_len == 0) build_packet();
    pend_off += bleuart.write(pend + pend_off, pend_len - pend_off);
    if (pend_off < pend_len) break;      // TX FIFO full, retry the rest next pass
    tx_pkts++;
    tx_bytes += pend_len;
    seq++;
    pend_len = 0;
  }
}
