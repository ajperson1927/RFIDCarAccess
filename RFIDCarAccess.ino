#include <EnergySaving.h>

#define OUTPUT_LED 9
#define INTERRUPT_PIN 8
#define FOB_POWER_PIN 7
#define FOB_UNLOCK_PIN 6
#define FOB_LOCK_PIN 5

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

 pinMode(0, OUTPUT);      //temporary 3.3v power until 
 digitalWrite(0, HIGH);   //hooked to external supply
 
 digitalWrite(FOB_POWER_PIN, LOW);
 digitalWrite(OUTPUT_LED, HIGH);

 energySaving.begin(WAKE_EXT_INTERRUPT, INTERRUPT_PIN, interruptRoutine);
}

void loop() {
  if (millis() - fobOnTime > fobOnPeriod) {
    digitalWrite(FOB_POWER_PIN, LOW);
    energySaving.standby();
  }
}

void interruptRoutine() {
  digitalWrite(FOB_POWER_PIN, HIGH);
  fobOnTime = millis();
}
