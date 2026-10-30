/* Binary echo fixture: configure SendCommand for 115200 baud, 8N1. */
void setup() { Serial.begin(115200); }
void loop() {
  while (Serial.available() > 0) {
    int byte_value = Serial.read();
    if (byte_value >= 0) Serial.write((unsigned char)byte_value);
  }
}
