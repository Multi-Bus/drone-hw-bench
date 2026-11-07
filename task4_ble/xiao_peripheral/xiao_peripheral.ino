// Task 4 Lv1: XIAO nRF52840 BLE peripheral, Nordic UART Service (NUS)
// Board: Seeed nRF52 Boards (non-mbed) > Seeed XIAO nRF52840
// Packet (one line, ASCII): "<seq>,<millis>\n"
#include <bluefruit.h>

BLEUart bleuart;
uint32_t seq = 0;

void setup() {
  Serial.begin(115200);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("XIAO1");            // 보드마다 다르게 (XIAO1, XIAO2, ...)
  bleuart.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void loop() {
  char buf[40];
  int n = snprintf(buf, sizeof buf, "%lu,%lu\n",
                   (unsigned long)seq++, (unsigned long)millis());
  bleuart.write((uint8_t*)buf, n);
  Serial.print(buf);                     // USB 시리얼 확인용 (2단계)
  delay(100);
}
