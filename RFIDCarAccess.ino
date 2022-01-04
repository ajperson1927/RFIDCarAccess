#include <EnergySaving.h>

#define OUTPUT_LED 9
#define INTERRUPT_PIN 8

EnergySaving energySaving;

bool interrupted = false;

void setup() {
  // put your setup code here, to run once:
 Serial.begin(9600);
 
 pinMode(LED_BUILTIN, OUTPUT);
 pinMode(OUTPUT_LED, OUTPUT);
 pinMode(INTERRUPT_PIN, INPUT);
 
 digitalWrite(LED_BUILTIN, LOW);
 digitalWrite(OUTPUT_LED, HIGH);

 energySaving.begin(WAKE_EXT_INTERRUPT, INTERRUPT_PIN, interruptRoutine);
}

void loop() {
  digitalWrite(9, HIGH);
  delay(100);
  digitalWrite(9, LOW);
  delay(100);
  if (millis() > 10000 && interrupted == false) {
    energySaving.standby();
    
    
    digitalWrite(9, HIGH);
  }
  
}

void interruptRoutine() {
    interrupted = true;
  }
