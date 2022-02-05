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
  
}

void loop() {
  
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
