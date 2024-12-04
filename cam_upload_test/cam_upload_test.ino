#include <WiFi.h>
#include <ArduCAM.h>
#include <Wire.h>
#include <SPI.h>
#include "memorysaver.h"
#include <Adafruit_PN532.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4);

#define PN532_SS 33
Adafruit_PN532 nfc(PN532_SS);

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

bool captureImage();
bool uploadImage ();

// Initialization
void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin();

  // lcd.begin(20, 4);
  // lcd.backlight();
  // lcd.setCursor(0,0);
  // lcd.print("Hello");

  // Initialize ArduCAM
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.OV5642_set_JPEG_size(OV5642_1024x768);

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");



  Serial.print("RC532 CS state befor init: ");

  int csState = digitalRead(PN532_SS); // Read the CS pin state
  Serial.print("CS pin state: ");
  if (csState == HIGH) {
    Serial.println("HIGH");
  } else {
    Serial.println("LOW");
  }

  // nfc.begin();
  
  Serial.print("RC532 CS state after init: ");

  int csState1 = digitalRead(PN532_SS); // Read the CS pin state
  Serial.print("CS pin state: ");
  if (csState1 == HIGH) {
    Serial.println("HIGH");
  } else {
    Serial.println("LOW");
  }
}

// Capture and upload image
void loop() {
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

  delay(5000); // Capture and upload every 10 seconds
}

bool captureImage() {
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  Serial.print("ArduCAM CS state before image: ");

  int csState = digitalRead(CS_PIN); // Read the CS pin state
  Serial.print("CS pin state: ");
  if (csState == HIGH) {
    Serial.println("HIGH");
  } else {
    Serial.println("LOW");
  }


  Serial.print("RC532 CS state before image: ");

  int csState1 = digitalRead(PN532_SS); // Read the CS pin state
  Serial.print("CS pin state: ");
  if (csState1 == HIGH) {
    Serial.println("HIGH");
  } else {
    Serial.println("LOW");
  }

  // Wait for capture to complete
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    delay(100);
  }

  Serial.print("RC532 CS state after image: ");

  int csState2 = digitalRead(PN532_SS); // Read the CS pin state
  Serial.print("CS pin state: ");
  if (csState2 == HIGH) {
    Serial.println("HIGH");
  } else {
    Serial.println("LOW");
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

// Upload image to server
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