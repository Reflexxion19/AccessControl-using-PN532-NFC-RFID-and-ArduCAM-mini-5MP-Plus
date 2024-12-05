#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

#define PN532_SS   (15)
Adafruit_PN532 nfc(PN532_SS);

uint8_t ID[16];
void getStringAsUint8Array(String str, uint8_t* result);

bool FirstTime = true;
String SerialID;

void setup(void) {
  Serial.begin(115200);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
}


void loop(void) {
  Serial.println("Write the ID that you want to write into the card:");
  if(!FirstTime)
    Serial.println("(if you want to write the same ID just press ENTER)");
  
  while(Serial.available() == 0){
  }
  String SerialString = Serial.readString();

  if(FirstTime){
    if(SerialString != "\n"){
      FirstTime = false;
      SerialID = SerialString;
      getStringAsUint8Array(SerialID, ID); // Write the ID into Serial to be written into the card
    }
    else{
      Serial.println("ID cannot be empty...");
      return;
    }
  }
  else{
    if(SerialString == "\n"){
      getStringAsUint8Array(SerialID, ID); // Write the ID into Serial to be written into the card
    }
    else if(SerialString.length() >= 1){
      SerialID = SerialString;
      getStringAsUint8Array(SerialID, ID); // Write the ID into Serial to be written into the card
    }
  }

  Serial.println("Waiting for an ISO14443A Card...");

  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    // Display some basic information about the card
    Serial.print("Found a Card  |  ");
    Serial.print("UID Length: "); Serial.print(uidLength, DEC); Serial.print(" bytes  |  ");
    Serial.print("UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");

    if (uidLength == 4)
    {
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

      // Authenticate block 3 for getting permission to write in sector 2
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);

      if (success)
      {
        uint8_t data[16];

        // Writing data in sector 2 block 5
        memcpy(data, ID, sizeof data);
        Serial.println("Writing to Block 5:");
        nfc.PrintHexChar(data, 16);
        Serial.println();
        success = nfc.mifareclassic_WriteDataBlock (5, data);

        // Try to read the contents of block 60
        success = nfc.mifareclassic_ReadDataBlock(5, data);

        if (success)
        {
          // Data seems to have been read ... spit it out
          Serial.println("Reading Block 5:");
          nfc.PrintHexChar(data, 16);
          Serial.println("");

          delay(2000);
        }
        else
        {
          Serial.println("Ooops ... unable to read the requested block");
        }
      }
      else
      {
        Serial.println("Ooops ... authentication failed");
      }
    }
  }
}

void getStringAsUint8Array(String str, uint8_t* result) {
  for (int i = 0; i < 16; i++) {
    result[i] = 0; // Initialize all to zero
  }

  for (int i = 0; i < str.length() - 1 && i < 16; i++) { // str.length() - 1, so that "/n" wouldn't be added to the array
    result[i] = (uint8_t)str[i];  // Convert each character to uint8_t
  }
}