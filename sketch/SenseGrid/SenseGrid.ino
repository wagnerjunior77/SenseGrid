#include <Arduino.h>
static const int PIN_OUT=4;    // O -> GPIO4
static const int RADAR_RX=1;   // T -> RX (ESP)
static const int RADAR_TX=3;   // R -> TX (ESP)
HardwareSerial Radar(1);

void sendFrame(uint8_t func, uint8_t c1, uint8_t c2,
               const uint8_t* data, size_t nData) {
  uint16_t len = 1 + 2 + nData + 1;                // func+2cmd+data+checksum
  uint8_t hdr[4] = {0x55,0x5A,(uint8_t)(len>>8),(uint8_t)len};
  uint32_t s=0; for(int i=0;i<4;i++) s+=hdr[i];
  uint8_t tmp[3+256]; size_t k=0;
  tmp[k++]=func; s+=func; tmp[k++]=c1; s+=c1; tmp[k++]=c2; s+=c2;
  for(size_t i=0;i<nData;i++){ tmp[k]=data[i]; s+=tmp[k]; k++; }
  uint8_t sum=(uint8_t)(s&0xFF);

  Radar.write(hdr,4);
  Radar.write(tmp,k);
  Radar.write(&sum,1);
}

void sendGetVersion(){ sendFrame(0x00,0x00,0x01,nullptr,0); } // checksum sai correto

void setup(){
  Serial.begin(115200);
  pinMode(PIN_OUT, INPUT_PULLDOWN);
  delay(400);
  Radar.begin(115200, SERIAL_8N1, RADAR_RX, RADAR_TX);
  Serial.println("\n[UART1] RX=GPIO1  TX=GPIO3  @115200  (CP2102 fora do caminho)");
}

void loop(){
  // OUT
  static int last=-1; int cur=digitalRead(PIN_OUT);
  if(cur!=last){ last=cur; Serial.printf("[OUT=%d] t=%lu\n",cur,(unsigned long)millis()); }

  // Ping versão a cada ~1.2s
  static uint32_t t0=0; if(millis()-t0>1200){ t0=millis(); sendGetVersion(); Serial.println("[ping] get-version"); }

  // Ler frames brutos e mostrar em HEX
  while(Radar.available()>=4){
    int b1=Radar.read(); if(b1!=0x55) continue;
    int b2=Radar.read(); if(b2!=0xA5 && b2!=0x5A) continue; // A5=radar->host, 5A=host->radar
    while(Radar.available()<2); uint8_t lh=Radar.read(), ll=Radar.read();
    uint16_t len=(lh<<8)|ll;
    while((int)Radar.available()<len);
    static uint8_t pl[260];
    for(uint16_t i=0;i<len;i++) pl[i]=Radar.read();

    Serial.print((b2==0xA5)?"[RX radar] ":"[echo host] ");
    Serial.printf("len=%u : 55 %02X %02X %02X ", (unsigned)len,(uint8_t)b2,lh,ll);
    for(uint16_t i=0;i<len;i++) Serial.printf("%02X ", pl[i]);
    Serial.println();
  }
}
