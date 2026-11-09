// Task 4 Lv2: throughput test, XIAO nRF52840 BLE peripheral, Nordic UART Service (NUS)
// Board: Seeed nRF52 Boards (non-mbed) > Seeed XIAO nRF52840 (Sense)
//
// USB serial commands (115200, one line each, sent by measure/throughput.py):
//   CFG <len> <ci_ms> [rate]   packet length in bytes (20..244), requested connection interval in ms,
//                              optional packets per second (omitted or 0 = as fast as possible)
//   START               reset counters, send packets as fast as the link allows
//   STOP                stop sending, print totals
//   STAT                print the connection's actual MTU, interval and data length
//
// Everything the central writes to the NUS RX characteristic is echoed back at once (RTT test).
//
// Auto mode (distance test, XIAO on a power bank, no PC): until the first serial line arrives, the XIAO
// starts sending 64 B x 50 packets/s at a 7.5 ms connection interval as soon as the central enables
// notifications, and resumes after a reconnect (seq keeps counting). Any serial line switches to manual mode.
// The blue LED shows the state: blinking = waiting for the central, on = connected.
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
uint32_t rate = 0;                       // packets per second, 0 = as fast as possible
uint32_t period_us = 0, next_us = 0;
bool running = false;
bool manual = false;                     // true after the first serial line: no auto mode
bool auto_started = false;               // auto mode has reset the counters once
static const uint16_t AUTO_LEN = 64, AUTO_CI_UNITS = 6;   // 6 x 1.25 ms = 7.5 ms
static const uint32_t AUTO_RATE = 50;
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
  if (!manual) running = false;          // auto mode restarts after the reconnect
}

void setup() {
  pinMode(LED_BLUE, OUTPUT);             // status LED (active low): blinking = waiting, on = connected
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
  if (!manual) { manual = true; running = false; }   // a PC is controlling: leave auto mode
  if (line.startsWith("CFG")) {
    // "CFG <len> <ci_ms>"; sscanf can't read floats in this core's newlib-nano
    int sp = line.indexOf(' ', 4);
    int sp2 = line.indexOf(' ', sp + 1);
    int len = line.substring(4, sp).toInt();
    float ci_ms = line.substring(sp + 1, sp2 < 0 ? line.length() : sp2).toFloat();
    int r = sp2 < 0 ? 0 : line.substring(sp2 + 1).toInt();   // packets per second, 0 = as fast as possible
    if (sp > 0 && len > 0 && ci_ms > 0 && r >= 0) {
      rate = r;
      period_us = r ? 1000000UL / r : 0;
      pkt_len = constrain(len, 20, PKT_MAX);
      ci_units = (uint16_t)(ci_ms / 1.25f + 0.5f);
      Bluefruit.Periph.setConnInterval(ci_units, ci_units);
      if (conn_hdl != BLE_CONN_HANDLE_INVALID) {
        Bluefruit.Connection(conn_hdl)->requestConnectionParameter(ci_units);
      }
      pend_len = 0;
      Serial.printf("OK CFG len=%u ci_units=%u rate=%lu\n", pkt_len, ci_units, (unsigned long)rate);
    } else {
      Serial.println("ERR CFG");
    }
  } else if (line == "START") {
    seq = tx_pkts = tx_bytes = 0;
    pend_len = 0;
    t_start = millis();
    next_us = micros();
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
  digitalWrite(LED_BLUE, conn_hdl == BLE_CONN_HANDLE_INVALID ? ((millis() / 500) & 1) : LOW);
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { handle_line(line); line = ""; }
    else if (c != '\r') line += c;
  }

  if (bleuart.available()) {             // echo for the RTT test (measure/rtt.py)
    uint8_t echo[PKT_MAX];
    int n = bleuart.read(echo, sizeof echo);
    bleuart.write(echo, n);
  }

  if (!manual && !running && conn_hdl != BLE_CONN_HANDLE_INVALID && bleuart.notifyEnabled()) {
    // The central has finished service discovery, so it is safe to ask for the interval now.
    pkt_len = AUTO_LEN;
    rate = AUTO_RATE;
    period_us = 1000000UL / AUTO_RATE;
    ci_units = AUTO_CI_UNITS;
    Bluefruit.Periph.setConnInterval(ci_units, ci_units);
    Bluefruit.Connection(conn_hdl)->requestConnectionParameter(ci_units);
    if (!auto_started) { seq = tx_pkts = tx_bytes = 0; t_start = millis(); auto_started = true; }
    pend_len = 0;
    next_us = micros();
    running = true;
  }

  if (!running || !bleuart.notifyEnabled()) return;

  for (int i = 0; i < 8; i++) {          // small burst per loop pass
    if (pend_len == 0) {
      if (period_us) {                   // fixed-rate mode: one packet per period
        if ((int32_t)(micros() - next_us) < 0) break;            // not due yet
        next_us += period_us;
        if ((int32_t)(micros() - next_us) > 1000000) next_us = micros();  // blocked for long: do not burst to catch up
      }
      build_packet();
    }
    pend_off += bleuart.write(pend + pend_off, pend_len - pend_off);
    if (pend_off < pend_len) break;      // TX FIFO full, retry the rest next pass
    tx_pkts++;
    tx_bytes += pend_len;
    seq++;
    pend_len = 0;
  }
}
