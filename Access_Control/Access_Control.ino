#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <ArduCAM.h>
#include "memorysaver.h"
#include <Adafruit_PN532.h>
#include <LiquidCrystal_I2C.h>

#define RELAY 32

#define BUZZER 33

#define PN532_SS 15
Adafruit_PN532 nfc(PN532_SS);

LiquidCrystal_I2C lcd(0x27,20,4);

// WiFi credentials
const char* ssid = "POCO X3 NFC";
const char* password = "duokivesiu";

// Database server information
const char* server = "78.61.96.88";
const int serverPort = 80; // Default HTTP port
const char* serverEndpoint = "/upload_image.php"; // PHP endpoint to handle image upload

#define CS_PIN 5 // Chip Select pin for SPI
ArduCAM myCAM(OV5642, CS_PIN);

// Buffer for image data
#define MAX_BUFFER_SIZE 60000
uint8_t buffer[MAX_BUFFER_SIZE];
uint32_t LENGTH;

void checkIDValidity(String ID);
bool captureImage();
bool uploadImage ();
String convertToString(uint8_t *data, size_t length);

int DoorOpenTime = 5000;

String ValidIDs[2] =
{
"Radzius",
"Jonaitis"
};

void setup() {
  Serial.begin(115200); // Initialize serial communication
  Wire.begin();
  SPI.begin();

  pinMode(RELAY, OUTPUT);

  pinMode(BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();

  // Initialize ArduCAM
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.OV5642_set_JPEG_size(OV5642_1024x768);
  //myCAM.OV5642_set_JPEG_size(OV5642_320x240);

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.println("Waiting for an ISO14443A Card ...");

  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("Pridekite ID");
  lcd.setCursor(0,2);
  lcd.print("kortele");
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    // Display some basic information about the card
    Serial.print("Found an ISO14443A card  |  ");
    Serial.print("UID Length: "); Serial.print(uidLength, DEC); Serial.print(" bytes  |  ");
    Serial.print("UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");

    if (uidLength == 4)
    {
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

      uint8_t data[16];

      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
      
      if(success){
        success = nfc.mifareclassic_ReadDataBlock(5, data);

        if (success){
          String string = convertToString(data, 16);

          checkIDValidity(string);
        }
      }
      else{
        Serial.print("Block authentication failed");
      }
    }
  }
  delay(2000);
}

void checkIDValidity(String ID){
  bool check = false;

  for (String i : ValidIDs){
    if(i == ID){
      check = true;
    }
  }

  if (check){
    Serial.println("Access Granted");
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Sveiki ");
    lcd.setCursor(0,2);
    lcd.print(ID);

    digitalWrite(RELAY, HIGH);
    delay(DoorOpenTime);
    digitalWrite(RELAY, LOW);
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Pridekite ID");
    lcd.setCursor(0,2);
    lcd.print("kortele");
  }
  else{
    Serial.println("Access Denied");
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Neteisingas ID");
    lcd.setCursor(0,3);
    lcd.print("Prieiga draudziama");

    tone(BUZZER, 200);

    Serial.println("Capturing image...");
    if (!captureImage()) {
      Serial.println("Failed to capture image.");
      delay(5000);
      return;
    }

    Serial.println("Uploading image...");
    if (!uploadImage()) {
      Serial.println("Failed to upload image.");
    } else {
      Serial.println("Image uploaded successfully!");
    }

    noTone(BUZZER);
    
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Pridekite ID");
    lcd.setCursor(0,2);
    lcd.print("kortele");
  }

  Serial.println();
  Serial.println("Waiting for an ISO14443A Card ...");
}

bool captureImage() {
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  // Wait for capture to complete
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    delay(100);
  }

  // Read image from FIFO
  LENGTH = myCAM.read_fifo_length();
  Serial.println(LENGTH);
  if (LENGTH > MAX_BUFFER_SIZE) {
    Serial.println("Error: Image size exceeds buffer size.");
    return false;
  }

  myCAM.CS_LOW();
  for (uint32_t i = 0; i < LENGTH; i++) {
    buffer[i] = myCAM.read_fifo();
  }
  myCAM.CS_HIGH();

  Serial.printf("Image captured. Size: %u bytes\n", LENGTH);
  return true;
}

bool uploadImage() {
  WiFiClient client;
  if (!client.connect(server, serverPort)) {
    Serial.println("Connection to server failed.");
    return false;
  }

  // Build HTTP POST request
  String boundary = "------------------------abcd1234";
  String header = "--" + boundary + "\r\n" +
                  "Content-Disposition: form-data; name=\"image\"; filename=\"image.jpg\"\r\n" +
                  "Content-Type: image/jpeg\r\n\r\n";

  String footer = "\r\n--" + boundary + "--\r\n";

  uint32_t contentLength = header.length() + LENGTH + footer.length();

  client.println("POST " + String(serverEndpoint) + " HTTP/1.1");
  client.println("Host: " + String(server));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(contentLength));
  client.println();
  client.print(header);

  // Send image data
  uint32_t bytesSent = 0;
  for (uint32_t i = 0; i < LENGTH; i++) {
    client.write(buffer[i]);

    bytesSent++;
    if (bytesSent % 1024 == 0) {
      Serial.printf("Sent %u bytes...\n", bytesSent); // Progress logging
    }
  }

  client.print(footer);

  // Wait for server response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readString();
  Serial.println("Server response:");
  Serial.println(response);

  return response.indexOf("SUCCESS") != -1;
}

String convertToString(uint8_t *data, size_t length) {
  String result = "";
  for (size_t i = 0; i < length; i++) {
    if (data[i] != 0) { // Skip null (padding) bytes
      result += (char)data[i]; // Convert byte to char and append to String
    }
  }
  return result;
}