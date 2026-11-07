// Task 4 Lv1 step 2: LED blink + USB serial
// 내장 LED는 active low (LOW = 켜짐)
#include <Adafruit_TinyUSB.h>   // 이 패키지에서 Serial이 링크 안 되는 문제 회피
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);  Serial.println("LED on");  delay(500);
  digitalWrite(LED_BUILTIN, HIGH); Serial.println("LED off"); delay(500);
}
