#include <EnergySaving.h>
#include <Adafruit_PN532.h>
#include <SPI.h>
#include <Wire.h> 
#include <FlashAsEEPROM.h>

#define FOB_UNLOCK_PIN 0
#define FOB_LOCK_PIN 1
#define OUTPUT_LED 6
#define FOB_POWER_PIN 7
#define INTERRUPT_PIN 8

#define IRQ_PIN 3
#define RESET_PIN 2
//#define SDA_PIN 4     These are hardcoded pins. Included
//#define SCL_PIN 5     for clarity only. Can't be set or changed

int fobOnPeriod = 30000;
unsigned long fobOnTime;

int tagScanWaitPeriod = 2000;
unsigned long tagScanTime;

//variables for EEEPROM and UID handling
const int maxUIDLength = 7; //The max length a UID can be. Most tags are 4 or 7 long
const int maxUIDCount = 10; //The max amount of UIDs can be stored in the EEPROM. Not including the master tag
const int firstUIDPosition = 1; //first UID after the master tag

const int TAGSCANNED =      0;
const int TAGWAITING =      1;
const int TAGIDLE =         2;

const int IDLESTATE =       0;
const int CARONSTATE =      1;
const int COUNTDOWNSTATE =  2;
const int ADDMASTERTAG =    3;
const int ADDNEWTAG =       4;
const int REMOVETAG =       5;
const int CLEARALLTAGS =    6;

int tagState = TAGIDLE;
int systemState = IDLESTATE;
bool newTag = false;

EnergySaving energySaving;
Adafruit_PN532 pn532(IRQ_PIN, RESET_PIN);

void setup() 
{
 
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

  //energySaving.begin(WAKE_EXT_INTERRUPT, INTERRUPT_PIN, interruptRoutine);
  pn532.begin();
  
  //If EEPROM doesn't exist yet, create and fill it with zeroes.
  //Set state to adding master tag
  if (!EEPROM.isValid()) { 
    for (int i = 0; i <= (maxUIDCount + 2) * 7; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    systemState = ADDMASTERTAG;
  }
}

void loop() 
{ /*
  uint32_t versiondata = pn532.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not detected");
    //while (1);
  }
  Serial.print("Version: ");
  Serial.println((versiondata>>24) & 0xFF, HEX);
  */

  uint8_t uid[maxUIDLength];
  bool validScan = scanTag(uid);

  switch(systemState)
  {
    case IDLESTATE:
    {
      if (validScan)
      {
        uint8_t masterUID[maxUIDLength];
        readFromEEPROM(masterUID, 0);
        if (uidsMatch(uid, masterUID)
        {
          systemState = ADDNEWTAG;
          break;
        }
      }
      break;
    }
  }
  
  if (millis() - fobOnTime > fobOnPeriod) 
  {
    //digitalWrite(LED_BUILTIN, LOW);
    //energySaving.standby();
  }

    //test code. halts loop until something is entered in serial monitor
    Serial.flush();
    while (!Serial.available());
    while (Serial.available()) 
    {
      Serial.read();
    }
    Serial.flush();
}

void interruptRoutine() 
{
  //digitalWrite(LED_BUILTIN, HIGH);
  //Serial.println("INTERRUPTED");
  fobOnTime = millis();
}

//This takes in a tag UID, writes it to the proper indexes
//in the EEPROM derived from uidIndex, and commits the data.
//Returns success status
bool writeUIDIntoEEPROM(uint8_t uid[], uint8_t uidIndex) 
{
  int index = (uidIndex) * maxUIDLength + 1;//calculate starting index
  
  for (int i = 0; i < maxUIDLength; i++) //Write uid[] into EEPROM
  {
    EEPROM.write(index + i, uid[i]);
  }
  EEPROM.commit();
  return true;
}

//This reads the tag UID from the proper indexes derived from
//uidIndex, and writes it to the given uid[].
//Returns success status
bool readUIDFromEEPROM(uint8_t uid[], uint8_t uidIndex) 
{
  
  //If EEPROM doesn't exist yet, do nothing and return false
  if (!EEPROM.isValid()) return false; 
  
  int index = uidIndex * maxUIDLength + 1; //calculate starting index
  
  for (int i = 0; i < maxUIDLength; i++) //Copy EEPROM UID into uid[]
  {
    uid[i] = EEPROM.read(index + i);
  }
  return true;
}

bool addUIDIntoEEPROMList(uint8_t uid[]) 
{
  int currentUIDCount = EEPROM.read(0);
  if (currentUIDCount >= maxUIDCount + 1) return false;
  
  EEPROM.write(0, currentUIDCount + 1);
  return writeUIDIntoEEPROM(uid, currentUIDCount);
}

//Removes given UID from the EEPROM, and shifts the UIDs after
//1 to the left 
bool removeUIDFromEEPROMList(uint8_t uid[]) 
{
  
  //Finds index of UID we're trying to remove. If it doesn't exist,
  //or is the master tag, do nothing and return false
  int uidIndex = searchUIDInEEPROM(uid);
  if (uidIndex <= 0) return false;

  //Shifts every UID left, starting with the one after target UID
  for (int i = uidIndex; i < maxUIDCount; i++) 
  {
     uint8_t newUID[maxUIDLength];
     readUIDFromEEPROM(newUID, i + 1);
     writeUIDIntoEEPROM(newUID, i);
  }
  EEPROM.write(0, uidIndex);
  EEPROM.commit();
  return true;
}

//Searches the EEPROM for given UID. If a match is found, 
//it's index is returned. Otherwise a -1 is returned
int searchUIDInEEPROM(uint8_t uid[]) 
{
  //iterate through all uids in list
  for (int i = 0; i < maxUIDCount; i++) 
  {
    uint8_t currentUID[maxUIDLength];
    readUIDFromEEPROM(currentUID, i);

    if (uidsMatch(currentUID, uid)) return i;
  }
  return -1;
}

//Compare given uids. Return whether they match or not. 
bool uidsMatch (uint8_t uid1[], uint8_t uid2[])
{
  bool arraysMatch = true;
  for (int i = 0; i < maxUIDLength; i++) 
  {
    if (uid1[i] != uid2[i]) 
    {
      arraysMatch = false;
      break;
    }
  }
  return arraysMatch;
}

bool scanTag(uint8_t uid[])
{
  uint8_t success;
  uint8_t newUid[maxUIDLength];
  uint8_t clearUid[] = {0,0,0,0,0,0,0};
  uint8_t uidLength;
  success = pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, newUid, &uidLength);

  switch (tagState)
  {
    case TAGIDLE:
    {
      if (success) 
      {
        tagState = TAGSCANNED;
        uid = newUid;
        digitalWrite(OUTPUT_LED, HIGH);
        return true;
        //pn532.PrintHex(uid, uidLength);
        //Serial.println(uid[1]);
      }
    }
    break;
    
    case TAGSCANNED:
    {
      if (!success)
      {
        tagState = TAGWAITING;
      }
      return false;
    }
    break;
    
    case TAGWAITING:
    {
      if (millis() - tagScanTime > tagScanWaitPeriod) 
      {
        tagState = TAGIDLE;
        digitalWrite(OUTPUT_LED, LOW);
      }
    }
    break;
  }
  return false;
}
