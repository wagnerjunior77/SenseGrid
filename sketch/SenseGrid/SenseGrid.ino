#include <Arduino.h>

// ---- Pinos no ESP32-C3
static const int RADAR_RX = 20;   // C3 recebe do radar (T do módulo)
static const int RADAR_TX = 21;   // C3 transmite pro radar (R do módulo)
static const int RADAR_OUT = 4;   // O do módulo (presença digital)

// ---- UART do radar
HardwareSerial RadarSerial(1); // use a UART “livre”

// Parser bem simples para o frame 0x55 0xA5 ... (relato ativo)
// Datasheet: header 0x55 0xA5, LEN(2, big-endian) = (func + cmd1 + cmd2 + data + checksum)
enum State { H0, H1, L0, L1, PAYLOAD };
State st = H0;
uint16_t need = 0;
uint8_t buf[64];
uint16_t idx = 0;

void resetParser() { st = H0; need = 0; idx = 0; }

void processFrame(const uint8_t *p, uint16_t n) {
  if (n < 4) return;
  uint8_t func = p[0];
  uint8_t cmd1 = p[1];
  uint8_t cmd2 = p[2];
  // dados = p[3 .. n-2], checksum = p[n-1]
  const uint8_t *data = p + 3;
  uint16_t dataLen = (n >= 4) ? (n - 4) : 0;
  uint8_t sum = 0;
  for (uint16_t i = 0; i < n - 1; ++i) sum += p[i];
  if (sum != p[n-1]) {
    Serial.println("[radar] checksum errado");
    return;
  }

  // Relato ativo tipicamente func=0x03, cmd1=0x81
  if (func == 0x03 && cmd1 == 0x81) {
    // Datasheet descreve Data[0..] com:
    // status(1), id(1), dist_cm(2, BE), vel_cm_s(2, BE, signed), dir(1), pitch(1), strength(2, BE)
    if (dataLen >= 10) {
      uint8_t status = data[0];        // 0: ninguém | 1: movimento | 2: presença (estático)
      uint8_t id     = data[1];
      uint16_t dist  = (uint16_t(data[2]) << 8) | data[3];
      int16_t speed  = int16_t((uint16_t(data[4]) << 8) | data[5]);
      int8_t  dirCos = int8_t(data[6]);
      int8_t  pitch  = int8_t(data[7]);
      uint16_t snr   = (uint16_t(data[8]) << 8) | data[9];

      Serial.printf("[radar] st=%u id=%u dist=%.2fm v=%.2f m/s cos=%d pitch=%d snr=%u\n",
                    status, id, dist / 100.0, speed / 100.0, dirCos, pitch, snr);
    } else {
      Serial.printf("[radar] frame curto: lenData=%u\n", dataLen);
    }
  } else {
    Serial.printf("[radar] func=0x%02X cmd=0x%02X%02X lenData=%u\n", func, cmd1, cmd2, dataLen);
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {}
  pinMode(RADAR_OUT, INPUT);

  // UART do radar
  RadarSerial.begin(115200, SERIAL_8N1, RADAR_RX, RADAR_TX);

  Serial.println("SenseGrid hello-radar (C3 + ME73MS01)");
  Serial.println("OUT=alto => presença; UART=relatos 0x55 0xA5 ...");
}

void loop() {
  // 1) leitura do pino OUT (digital)
  static uint32_t tLast = 0;
  if (millis() - tLast > 500) {
    tLast = millis();
    int out = digitalRead(RADAR_OUT);
    Serial.printf("[OUT] %s\n", out ? "PRESENÇA" : "vazio");
  }

  // 2) parser do protocolo UART
  while (RadarSerial.available()) {
    uint8_t b = RadarSerial.read();
    switch (st) {
      case H0: st = (b == 0x55) ? H1 : H0; break;
      case H1: st = (b == 0xA5) ? L0 : ((b == 0x55) ? H1 : H0); break;
      case L0: need = (uint16_t(b) << 8); st = L1; break;
      case L1: need |= b; idx = 0; st = PAYLOAD; break;
      case PAYLOAD:
        if (idx < sizeof(buf)) buf[idx++] = b;
        if (idx >= need) { processFrame(buf, idx); resetParser(); }
        break;
    }
  }
}
