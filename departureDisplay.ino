#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "RTClib.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "SSD1306.h"

WiFiClientSecure client;
SSD1306  display(0x3c, 5, 4);

const char* ssid     = "affenstunk";
const char* password = "Schluss3ndlichkeit";

const char* host = "www.mvg.de";
const char* url = "fahrinfo/api/departure/1109?footway=0";
const char* apiKey = "5af1beca494712ed38d313714d4caff6";

const size_t capacity = 
  JSON_ARRAY_SIZE(2) + 
  JSON_ARRAY_SIZE(20) + 
  JSON_OBJECT_SIZE(2) + 
  2*JSON_OBJECT_SIZE(6) + 
  20*JSON_OBJECT_SIZE(8) +
  3000;

const String request = String("GET https://") + host + "/" + url + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" +
             "X-MVG-Authorization-Key: " + apiKey + "\r\n" +
             "Connection: close\r\n\r\n";

BLECharacteristic *pCharacteristic;

struct Connection {
   const char* lineNumber;
   const char* destination;
   TimeSpan timespanToDeparture;
};

struct Connection connections[30];
int numOfConnections;
String displayText;

DateTime serverTime;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

void setup() {
  Serial.begin(115200);
  initDisplay();
  connectToWifi();
  initBluetoothLowEnergy();
}

boolean isRelevantDestination(String destination) {
  return (destination == "Ostbahnhof" ||
    destination == "Messestadt West" ||
    destination == "Karlsplatz (Stachus)" ||
    destination == "Trudering Bf." ||
    destination == "Giesing Bf." ||
    destination == "Ostfriedhof");
}

void initDisplay() {
  display.init();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void connectToWifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
  }
}

void initBluetoothLowEnergy() {
  BLEDevice::init("MyESP32");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void loop() {
  display.setPixel(127,63);
  display.display();
  Serial.println("requesting departures");
  getDepartures();
  Serial.println("drawing departures");
  createDisplayText();
  drawTextOnDisplay();
  sendDeparturesViaBle();
  delay(10000);
}

void getDepartures() {
  if (!connectAndSendRequest()) return;
  if (!isStatusOk) return;
  Serial.println("getting server time");
  getDateFromHeader();
  //Serial.println("Server time: " + String(serverTime));
  showDate("time: ", serverTime);
  if (!jumpToEndOfHeaders()) return;
  Serial.println("processing response");
  extractDeparturesFromResponse();
}

boolean connectAndSendRequest() {
  if (!client.connect(host, 443)) {
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
  if (client.find((char *)"\r\nDate: ") && client.readBytes(buf, 5) == 5) {
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
  for(int i = 0; i < sizeof(departures); i++) {
    const char* destination = departures[i][F("destination")].as<char*>();
    Serial.println(String(destination));
    if (isRelevantDestination(destination)) {
      String departureTime = departures[i][F("departureTime")].as<String>().substring(0,10);
      Serial.print(F("Dep time: "));
      Serial.println(departureTime);
      DateTime departureDateTime (departureTime.toInt());
      String lineNumber = departures[i][F("label")].as<char*>();
      Serial.print(F("Line number: "));
      Serial.println(lineNumber);
      Serial.print(F("Destination: "));
      Serial.println(destination);
      showDate("Server time", serverTime);
      showDate("Departure time", departureDateTime);
      connections[numOfConnections].lineNumber = departures[i][F("label")].as<char*>();
      connections[numOfConnections].destination = destination;
      connections[numOfConnections].timespanToDeparture = departureDateTime - serverTime;
      numOfConnections++;
    }
  }
  client.stop();
}

JsonObject& fetchResponse() {
  StaticJsonBuffer<capacity> jsonBuffer;
  JsonObject& response = jsonBuffer.parseObject(client);
  if (!response.success()) {
    Serial.println(F("Parsing failed!"));
  }
  return response;
}

void createDisplayText() {
  displayText = "";
  for(int i=0; i<numOfConnections && i<2; i++){
    displayText += String(connections[i].lineNumber) + ": " + connections[i].timespanToDeparture.minutes() + " min";
    Serial.println();
    Serial.println(F("Line: "));
    Serial.println(connections[i].lineNumber);
    Serial.println(F("Destination: "));
    Serial.println(connections[i].destination);
    showTimeSpan("Timespan to departure: ", connections[i].timespanToDeparture);
    displayText += "\n";
  }
  Serial.println();
  Serial.println("==========================");
}

void drawTextOnDisplay() {
  char buf[64];
  displayText.toCharArray(buf, 64);

  display.clear();
  display.drawString(0, 0, buf);
  display.display();
}

void sendDeparturesViaBle() {
  Serial.println("DisplayText: ");
  Serial.println(displayText);
  char buf[64];
  displayText.toCharArray(buf, 64);
  Serial.println("Buf: ");
  Serial.println(buf);
  pCharacteristic->setValue(buf);
  pCharacteristic->notify();
}

void showDate(const char* txt, const DateTime& dt) {
    Serial.print(txt);
    Serial.print(' ');
    Serial.print(dt.year(), DEC);
    Serial.print('/');
    Serial.print(dt.month(), DEC);
    Serial.print('/');
    Serial.print(dt.day(), DEC);
    Serial.print(' ');
    Serial.print(dt.hour(), DEC);
    Serial.print(':');
    Serial.print(dt.minute(), DEC);
    Serial.print(':');
    Serial.print(dt.second(), DEC);
    
    Serial.print(" = ");
    Serial.print(dt.unixtime());
    Serial.print("s / ");
    Serial.print(dt.unixtime() / 86400L);
    Serial.print("d since 1970");
    
    Serial.println();
}

void showTimeSpan(const char* txt, const TimeSpan& ts) {
    Serial.print(txt);
    Serial.print(" ");
    Serial.print(ts.days(), DEC);
    Serial.print(" days ");
    Serial.print(ts.hours(), DEC);
    Serial.print(" hours ");
    Serial.print(ts.minutes(), DEC);
    Serial.print(" minutes ");
    Serial.print(ts.seconds(), DEC);
    Serial.print(" seconds (");
    Serial.print(ts.totalseconds(), DEC);
    Serial.print(" total seconds)");
    Serial.println();
}
