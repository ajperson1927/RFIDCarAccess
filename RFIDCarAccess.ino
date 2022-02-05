#include <EnergySaving.h>
#include <Adafruit_PN532.h>
#include <SPI.h>
#include <Wire.h> 

#define FOB_UNLOCK_PIN 0
#define FOB_LOCK_PIN 1
#define OUTPUT_LED 6
#define FOB_POWER_PIN 7
#define INTERRUPT_PIN 8

#define IRQ_PIN 2
#define RESET_PIN 3
//#define SDA_PIN 4     These are hardcoded pins. Included
//#define SCL_PIN 5     for clarity only. Can't be set or changed

int fobOnPeriod = 10000;
unsigned long fobOnTime = 0;

EnergySaving energySaving;
Adafruit_PN532 pn532(IRQ_PIN, RESET_PIN);

void setup() {
 
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(OUTPUT_LED, OUTPUT);
  pinMode(INTERRUPT_PIN, INPUT);
  pinMode(FOB_POWER_PIN, OUTPUT);
  pinMode(FOB_UNLOCK_PIN, OUTPUT);
  pinMode(FOB_LOCK_PIN, OUTPUT);
 
  //digitalWrite(LED_BUILTIN, HIGH);
  //digitalWrite(OUTPUT_LED, HIGH);

  Serial.begin(9600);
  while (!Serial);

  energySaving.begin(WAKE_EXT_INTERRUPT, INTERRUPT_PIN, interruptRoutine);
  pn532.begin();
  
}

void loop() {

  uint32_t versiondata = pn532.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("PN532 not detected");
    //while (1);
  }
  Serial.print((versiondata>>24) & 0xFF, HEX);

  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0};
  uint8_t uidLength;
  success = pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    pn532.PrintHex(uid, uidLength);
    Serial.println(uid[1]);
  }
  
  if (millis() - fobOnTime > fobOnPeriod) {
    //digitalWrite(LED_BUILTIN, LOW);
    //energySaving.standby();
  }
}

void interruptRoutine() {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("INTERRUPTED");
  fobOnTime = millis();
}
