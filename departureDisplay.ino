#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "RTClib.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "SSD1306.h"

// constants
const char* SSID PROGMEM = "affenstunk";
const char* PASSWORD PROGMEM = "Schluss3ndlichkeit";
const char* HOST PROGMEM = "www.mvg.de";
const char* URL PROGMEM = "fahrinfo/api/departure/1109?footway=0";
const char* API_KEY PROGMEM = "5af1beca494712ed38d313714d4caff6";
const char* SERVICE_UUID PROGMEM = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const char* CHARACTERISTIC_UUID PROGMEM = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const char* BLE_DEVICE_NAME PROGMEM = "Departures";
const size_t CAPACITY PROGMEM = 
  JSON_ARRAY_SIZE(2) + 
  JSON_ARRAY_SIZE(20) + 
  JSON_OBJECT_SIZE(2) + 
  2*JSON_OBJECT_SIZE(6) + 
  20*JSON_OBJECT_SIZE(8) +
  3000;
const String request PROGMEM = String(F("GET https://")) + HOST + "/" + URL + " HTTP/1.1\r\n" +
             "Host: " + HOST + "\r\n" +
             "X-MVG-Authorization-Key: " + API_KEY + "\r\n" +
             "Connection: close\r\n\r\n";

struct Connection {
   const char* lineNumber;
   const char* destination;
   TimeSpan timespanToDeparture;
};

// global variables
WiFiClientSecure client;
BLECharacteristic *pCharacteristic;
DateTime serverTime;
struct Connection connections[30];
int numOfConnections;
SSD1306  display(0x3c, 5, 4);
char displayText[64];

void setup() {
  Serial.begin(115200);
  initDisplay();
  connectToWifi();
  initBluetoothLowEnergy();
}

boolean isRelevantDestination(String destination) {
  return (destination == F("Ostbahnhof") ||
    destination == F("Messestadt West") ||
    destination == F("Karlsplatz (Stachus)") ||
    destination == F("Trudering Bf.") ||
    destination == F("Giesing Bf.") ||
    destination == F("Ostfriedhof"));
}

void initDisplay() {
  display.init();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void connectToWifi() {
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
  }
}

void initBluetoothLowEnergy() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void loop() {
  display.drawLine(0,63,127,63);
//  display.setPixel(127,63);
  display.display();
  Serial.println(F("requesting departures"));
  getDepartures();
  Serial.println(F("drawing departures"));
  createDisplayText();
  drawTextOnDisplay();
  sendDeparturesViaBle();
  delay(10000);
}

void getDepartures() {
  if (!connectAndSendRequest()) return;
  if (!isStatusOk) return;
  Serial.println(F("getting server time"));
  getDateFromHeader();
  if (!jumpToEndOfHeaders()) return;
  Serial.println(F("processing response"));
  extractDeparturesFromResponse();
}

boolean connectAndSendRequest() {
  if (!client.connect(HOST, 443)) {
    Serial.println(F("Failed to connect"));
    return false;
  }
  client.print(request);
  
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    return false;
  }

  return true;
}

boolean isStatusOk() {
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 200") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return false;
  }
  return true;
}

void getDateFromHeader() {
  char buf[5];
  if (client.find("\r\nDate: ") && client.readBytes(buf, 5) == 5) {
    int day = client.parseInt();    // day
    client.readBytes(buf, 1);    // discard
    client.readBytes(buf, 3);    // month
    int year = client.parseInt();    // year
    int hour = client.parseInt();   // hour
    int minute = client.parseInt(); // minute
    int second = client.parseInt(); // second
    int month;
    switch (buf[0]) {
      case 'F':
        month =  2; 
        break; // Feb
      case 'S': 
        month =  9; 
        break; // Sep
      case 'O':
        month =  10; 
        break; // Oct
      case 'N':
        month =  11; 
        break; // Nov
      case 'D':
        month =  12; 
        break; // Dec
      default:
        if (buf[0] == 'J' && buf[1] == 'a') {
          month =  1;   // Jan
        }
        else if (buf[0] == 'A' && buf[1] == 'p') {
          month =  4;    // Apr
        }
        else switch (buf[2]) {
          case 'r': month =  3;
          break; // Mar
          case 'y': month =  5;
          break; // May
          case 'n': month =  6;
          break; // Jun
          case 'l': month =  7;
          break; // Jul
          default: // add a default label here to avoid compiler warning
          case 'g': month =  8;
          break; // Aug
        }
    }
    serverTime = DateTime (year, month, day, hour, minute, second);
  }
}

boolean jumpToEndOfHeaders() {
  if (!client.find("\r\n\r\n")) {
    Serial.println(F("Invalid response"));
    return false;
  }
  return true;  
}

void extractDeparturesFromResponse() {
  numOfConnections = 0;
  JsonObject& response = fetchResponse();
  JsonArray& departures = response[F("departures")];
  Serial.print(F("Number of departures: "));
  Serial.println(sizeof(departures));
  for(int i = 0; i < sizeof(departures); i++) {
    const char* destination = departures[i][F("destination")].as<char*>();
    if (isRelevantDestination(destination)) {
      String departureTime = departures[i][F("departureTime")].as<String>().substring(0,10);
      DateTime departureDateTime (departureTime.toInt());
      String lineNumber = departures[i][F("label")].as<char*>();
      connections[numOfConnections].lineNumber = departures[i][F("label")].as<char*>();
      connections[numOfConnections].destination = destination;
      connections[numOfConnections].timespanToDeparture = departureDateTime - serverTime;
      numOfConnections++;
    }
  }
  Serial.print(F("Number of connections: "));
  Serial.println(numOfConnections);
  client.stop();
}

JsonObject& fetchResponse() {
  StaticJsonBuffer<CAPACITY> jsonBuffer;
  JsonObject& response = jsonBuffer.parseObject(client);
  if (!response.success()) {
    Serial.println(F("Parsing failed!"));
  }
  return response;
}

void createDisplayText() {
  displayText[0] = '\0';
  for(int i=0; i<numOfConnections && i<2; i++){
    char row[32];
    sprintf(row, "%s: %d min\n", connections[i].lineNumber, connections[i].timespanToDeparture.minutes());
    strcat(displayText, row);
  }
  Serial.println(F("------------"));
  Serial.println(displayText);
  Serial.println(F("------------"));
}

void drawTextOnDisplay() {
  display.clear();
  display.drawString(0, 0, displayText);
  display.display();
}

void sendDeparturesViaBle() {
  pCharacteristic->setValue(displayText);
  pCharacteristic->notify();
}
