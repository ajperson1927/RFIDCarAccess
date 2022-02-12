/*
      This is a program that allows one to replace a key fob on a push button car with an RFID access control system.
      It utilizes a Seeeduino XIAO as the main board, with a PN532 as the RFID antenna. Tag storing is handled by
      using a simulated EEPROM in fllash storage. Due to it being flash storage, it resets every time the program is
      uploaded. 
      The EEPROM contains a simulated array to contain the tag UIDs. The first byte indicates the size of the array.
      The rest is divided into maxUIDLength byte long UIDs. First one will always be the master tag
*/
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
#define CAR_POWER_PIN -1 //Temporary pin position. Will be changed once position is decided on board

#define IRQ_PIN 2
#define RESET_PIN 3
//#define SDA_PIN 4     These are hardcoded pins. Included
//#define SCL_PIN 5     for clarity only. Can't be set or changed

//These are for tracking different timers the system uses
int fobOnPeriod = 30000; //How long the fob stays on without input before turning off
unsigned long fobOnTime;

int tagScanWaitPeriod = 2000; //How long to wait after scanning a tag before another can be scanned
unsigned long tagScanTime;

int changeTagTimeout = 5000; //How long must pass when adding/removing a tag before the action is auto cancelled
unsigned long tagTimeoutTime;

//variables for EEEPROM and UID handling
const int maxUIDLength = 7; //The max length a UID can be. Most tags are 4 or 7 long
const int maxUIDCount = 10; //The max amount of UIDs can be stored in the EEPROM. Not including the master tag
const int firstUIDPosition = 1; //first UID after the master tag

//States for when tags are being scanned
const int TAGWAITING =      0;
const int TAGIDLE =         1;
int tagState = TAGIDLE; //Starting state

//Stated for the main operation of the system
const int IDLESTATE =       0;
const int CARONSTATE =      1;
const int COUNTDOWNSTATE =  2;
const int ADDMASTERTAG =    3;
const int ADDNEWTAG =       4;
const int REMOVETAG =       5;
const int CLEARALLTAGS =    6;
int systemState = IDLESTATE; //Starting state

uint8_t clearUid[maxUIDLength]; //UID array full of zeroes

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
  pinMode(CAR_POWER_PIN, INPUT);

  Serial.begin(9600); //Debug purposes only. Won't be needed when finished
  while (!Serial);

  //energySaving.begin(WAKE_EXT_INTERRUPT, INTERRUPT_PIN, interruptRoutine);
  
  //If EEPROM doesn't exist yet, initialize it
  if (!EEPROM.isValid()) { 
    clearEEPROM();
  }

  pn532.begin();
  pn532.setPassiveActivationRetries(0); //Sets timeout to 0. Program will never halt waiting for tag scan
  pn532.SAMConfig(); //Sets to read passive tags

  //Prints firmware version. More for debugging purposes
  uint32_t versiondata = pn532.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not detected");
    while (1);
  }
  Serial.print("Version: ");
  Serial.println((versiondata>>24) & 0xFF, HEX);

  //Creates an empty uid to copy from with memcpy
  for (int i = 0; i < maxUIDLength; i++)
  {
    clearUid[i] = 0;
  }
}

void loop() 
{ 
  
  //Creates an empty uid, then checks for a valid tag scan. If found, empty uid is set to that tag
  uint8_t uid[maxUIDLength];
  memcpy(uid, clearUid, sizeof(uid));
  bool validScan = scanTag(uid);


  switch(systemState)
  {
    case IDLESTATE:
    {
      if (validScan)
      {
        //Find out if tag exists in EEPROM
        int uidIndex = searchUID(uid);

        //If not, immediately exit
        if (uidIndex < 0) 
        {
          break;
        }
        //If tag is master tag, go to the state for adding a new tag 
        else if (uidIndex == 0) 
        {
          systemState = ADDNEWTAG;
          tagTimeoutTime = millis();
          break;
        } 
        //If tag exists and isn't master tag, power up the key fob and press unlock button
        else 
        {
          systemState = COUNTDOWNSTATE;
          digitalWrite(FOB_POWER_PIN, HIGH);
          delay(100);
          digitalWrite(FOB_UNLOCK_PIN, HIGH);
          delay(100);
          digitalWrite(FOB_UNLOCK_PIN, LOW);
          fobOnTime = millis();
          break;
        }
      }
      break;
    }
    case ADDMASTERTAG:
    {
      //New master tag being added. If any tag is scanned, set it to master tag and go back to idle state
      if (validScan)
      {
        changeUID(uid, 0);
        systemState = IDLESTATE;
      }
      break;
    }
    case ADDNEWTAG:
    {
      //If timed out, go back to idle
      if (millis() - tagTimeoutTime > changeTagTimeout)
      {
        systemState = IDLESTATE;
      }
      
      if (validScan)
      {
        int uidIndex = searchUID(uid);
        //If master tag is scanned instead of a new tag, go to remove tag state
        if (uidIndex == 0)
        {
          tagTimeoutTime = millis();
          systemState = REMOVETAG;
        } 
        //If tag doesn't exist and isn't master tag, add it to the system
        else if (uidIndex < 0)
        {
          addUID(uid);
          systemState = IDLESTATE;
        }
        //If tag already exists, do nothing 
        else
        {
          systemState = IDLESTATE;
        }
      }
      break;
    }
    case REMOVETAG:
    {
      //If timed out, go back to idle
      if (millis() - tagTimeoutTime > changeTagTimeout)
      {
        systemState = IDLESTATE;
      }
      if (validScan)
      {
        int uidIndex = searchUID(uid);
        //If master tag scanned, start reset proccess
        if (uidIndex == 0)
        {
          systemState = CLEARALLTAGS;
        }
        //If tag doesn't exist, return to idle state
        else if (uidIndex == -1)
        {
          systemState = IDLESTATE;
        }
        //If tag exists, remove it and return to idle state
        else
        {
          removeUID(uid);
          systemState = IDLESTATE;
        }
      }
      
      break;
    }
    case COUNTDOWNSTATE:
    {
      //If a tag is scanned or time runs out, lock the car, turn off the fob, and return to idle state
      if (validScan || (millis() - fobOnTime > fobOnPeriod))
      {
          digitalWrite(FOB_LOCK_PIN, HIGH);
          delay(100);
          digitalWrite(FOB_LOCK_PIN, LOW);
          delay(100);
          digitalWrite(FOB_POWER_PIN, LOW);
          systemState = IDLESTATE;
      } 
      //If the car turns on, go to the car on state
      else if (digitalRead(CAR_POWER_PIN))
      {
        systemState = CARONSTATE;
      }
      break;
    }
    case CLEARALLTAGS:
    {
      //If timed out, go back to idle
      if (millis() - tagTimeoutTime > changeTagTimeout)
      {
        systemState = IDLESTATE;
      }
      if (validScan)
      {
        //If master tag is scanned, reset system. Otherwise wait for timeout
        int uidIndex = searchUID(uid);
        if (uidIndex == 0)
        {
          clearEEPROM();
        }
      }
      break;
    }
    case CARONSTATE:
    {
      //If car turns off, go to countdown state
      if (!digitalRead(CAR_POWER_PIN))
      {
        fobOnTime = millis();
        systemState = COUNTDOWNSTATE;
      }
      break;
    }
  }
}

void interruptRoutine() 
{
  //digitalWrite(LED_BUILTIN, HIGH);
  //Serial.println("INTERRUPTED");
  //fobOnTime = millis();
}

//This takes in a tag UID, writes it to the proper indexes in the EEPROM derived from uidIndex, and commits the data. Returns success status
bool changeUID(uint8_t uid[], uint8_t uidIndex) 
{
  int index = (uidIndex) * maxUIDLength + 1;//calculate starting index
  
  for (int i = 0; i < maxUIDLength; i++) //Write uid[] into EEPROM
  {
    EEPROM.write(index + i, uid[i]);
  }
  EEPROM.commit();
  Serial.println("WRITING TO EEPROM");
  return true;
}

//This reads the tag UID from the proper indexes derived from uidIndex, and writes it to the given uid[]. Returns success status
bool readUID(uint8_t uid[], uint8_t uidIndex) 
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

bool addUID(uint8_t uid[]) 
{
  int latestUIDIndex = EEPROM.read(0);
  if (latestUIDIndex >= maxUIDCount) return false;
  
  EEPROM.write(0, latestUIDIndex + 1);
  return changeUID(uid, latestUIDIndex + 1);
}

//Removes given UID from the EEPROM, and shifts the UIDs after 1 to the left 
bool removeUID(uint8_t uid[]) 
{
  
  //Finds index of UID we're trying to remove. If it doesn't exist,
  //or is the master tag, do nothing and return false
  int uidIndex = searchUID(uid);
  if (uidIndex <= 0) return false;

  //Shifts every UID left, starting with the one after target UID
  for (int i = uidIndex; i < maxUIDCount; i++) 
  {
     uint8_t newUID[maxUIDLength];
     readUID(newUID, i + 1);
     changeUID(newUID, i);
  }
  EEPROM.write(0, EEPROM.read(0) - 1);
  Serial.println("WRITING TO EEPROM");
  EEPROM.commit();
  return true;
}

//Searches the EEPROM for given UID. If a match is found, its index is returned. Otherwise a -1 is returned
int searchUID(uint8_t uid[]) 
{
  //iterate through all uids in list
  for (int i = 0; i < maxUIDCount; i++) 
  {
    uint8_t currentUID[maxUIDLength];
    
    readUID(currentUID, i);
    
    if (doUidsMatch(currentUID, clearUid)) return -1; //If scanned uid is empty, always reject it
    if (doUidsMatch(currentUID, uid)) return i;
  }
  return -1;
}

//Compare given uids. Return whether they match or not. 
bool doUidsMatch (uint8_t uid1[], uint8_t uid2[])
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

//Will scan tag and copy it to uid[] input. If tag was scanned within tagScanTime, ignore it
bool scanTag(uint8_t uid[])
{
  switch (tagState)
  {
    //Waiting for a tag scan
    case TAGIDLE:
    {
      boolean success;
      uint8_t newUid[maxUIDLength];//Input uid for pn532 scan. Will be set to scanned tag id
      uint8_t uidLength; //Useless, but required by pn532 library
      success = pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, &newUid[0], &uidLength);
      if (success) 
      {
        //Tag was scanned. Enter waiting state and start timer. Copy scanned uid to input uid[]
        tagState = TAGWAITING;
        memcpy(uid, newUid, sizeof(uid));
        tagScanTime = millis();
        digitalWrite(OUTPUT_LED, HIGH);
        
        return true;
      }
      break;
    }
    case TAGWAITING:
      if (millis() - tagScanTime > tagScanWaitPeriod) 
      {
        //If enough time has elapsed, enter idle state so tags can be scanned again
        tagState = TAGIDLE;
        digitalWrite(OUTPUT_LED, LOW);
      }
      break;
    default:
      break;
  }
  return false;
}
//Empty EEPROM and set state to adding master tag
void clearEEPROM()
{
    for (int i = 0; i <= (maxUIDCount + 2) * 7; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    systemState = ADDMASTERTAG;
}