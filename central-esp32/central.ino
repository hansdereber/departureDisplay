#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "RTClib.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// constants
const char *SSID PROGMEM = "affenstunk";
const char *PASSWORD PROGMEM = "Schluss3ndlichkeit";
const char *HOST PROGMEM = "www.mvg.de";
const char *URL PROGMEM = "fahrinfo/api/departure/634?footway=0";
const char *API_KEY PROGMEM = "5af1beca494712ed38d313714d4caff6";
const char *SERVICE_UUID PROGMEM = "2e80f571-d90f-4a75-818b-e90434e5ffaa";
const char *DISPLAY_TEXT_UUID PROGMEM = "c623addf-a88a-4d26-b78e-15baf8195cdd";
const char *BLE_DEVICE_NAME PROGMEM = "departures central";
const char *PERIPHERAL_ADDRESS PROGMEM = "e9:fa:f5:f5:4f:ba";
const size_t CAPACITY PROGMEM =
    JSON_ARRAY_SIZE(4) 
    + JSON_ARRAY_SIZE(18) 
    + JSON_OBJECT_SIZE(2) 
    + 4*JSON_OBJECT_SIZE(6) 
    + 18*JSON_OBJECT_SIZE(8) 
    + 3500; // calculated with https://arduinojson.org/v6/assistant/ and a little head room

const String request PROGMEM = String(F("GET https://")) + HOST + "/" + URL + " HTTP/1.1\r\n" +
                               "Host: " + HOST + "\r\n" +
                               "X-MVG-Authorization-Key: " + API_KEY + "\r\n" +
                               "Connection: close\r\n\r\n";

struct Connection
{
  const char *line;
  uint16_t destinationId;
  uint32_t departureTime;
  uint8_t minutesToDeparture;
};
struct Connection connections[2];

struct Map
{
  uint16_t stationId;
  const char *name;
};
struct Map stationMap[3];

// global variables
WiFiClientSecure client;
DateTime serverTime;
int numOfConnections;
static BLEAddress *pPeripheralAddress;
BLECharacteristic *pTxCharacteristic;
BLEClient *pPeripheral;
static boolean doConnect = false;
static boolean minutesChanged = true;
uint8_t restartCounter = 0;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    if (advertisedDevice.getAddress().toString() == PERIPHERAL_ADDRESS)
    {
      advertisedDevice.getScan()->stop();
      pPeripheralAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  }
};

void setup()
{
  initStationMap();
  Serial.begin(115200);
  initBluetoothLowEnergy();
}

void initStationMap()
{
  stationMap[0].stationId = 34;
  stationMap[0].name = "Nordbad";
  stationMap[1].stationId = 670;
  stationMap[1].name = "Arabellapark";
  stationMap[2].stationId = 1250;
  stationMap[2].name = "Messestadt West";
  // stationMap[3].stationId = 920;
  // stationMap[3].name = "Trudering Bf.";
  // stationMap[4].stationId = 1110;
  // stationMap[4].name = "Giesing Bf.";
  // stationMap[5].stationId = 1105;
  // stationMap[5].name = "Ostfriedhof";
}

boolean isRelevantDestination(const char *destination)
{
  for (int i = 0; i < sizeof(stationMap) / sizeof(stationMap[0]); i++)
  {
    if (strcmp(stationMap[i].name, destination) == 0)
    {
      return true;
    }
  }
  return false;
}

void checkAndConnectToWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(F("Reconnect with WiFi"));
    connectToWifi();
  }
}

void connectToWifi()
{
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  Serial.println(F("Connection with WiFi established"));
}

void initBluetoothLowEnergy()
{
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  pService->start();
}

void loop()
{
  checkAndConnectToBle();
  checkAndConnectToWifi();
  Serial.println(F("Requesting departures"));
  if (getDepartures() && minutesChanged)
  {
    Serial.println(F("Sending departures"));
    sendDeparturesViaBle();
  }
  checkResetCounter();
  delay(10000);
}

void checkAndConnectToBle()
{
  if (doConnect == true)
  {
    connectToServer(*pPeripheralAddress);
    doConnect = false;
  }
  if (!pPeripheral->isConnected())
  {
    Serial.println(F("Peripheral not connected"));
    restartCounter++;
  }
}

void checkResetCounter()
{
  Serial.print(F("RestartCounter: "));
  Serial.print(String(restartCounter));
  Serial.println(F("/6"));

  if (restartCounter > 6)
  {
    Serial.println(F("Restarting ESP"));
    ESP.restart();
  }
}

bool connectToServer(BLEAddress pAddress)
{
  Serial.print(F("Forming a connection to "));
  Serial.println(pAddress.toString().c_str());
  pPeripheral = BLEDevice::createClient();
  pPeripheral->connect(pAddress);
}

boolean getDepartures()
{
  if (!connectAndSendRequest())
    return false;
  Serial.println(F("Checking response status"));
  if (!isStatusOk())
    return false;
  Serial.println(F("Getting server time"));
  if (!getDateFromHeader())
    return false;
  Serial.println(F("Jumping to end of headers"));
  if (!jumpToEndOfHeaders())
    return false;
  Serial.println(F("Processing response"));
  if (!extractDeparturesFromResponse())
    return false;
  return true;
}

boolean connectAndSendRequest()
{
  client.stop();

  if (!client.connect(HOST, 443))
  {
    Serial.println(F("Failed to connect"));
    restartCounter++;
    return false;
  }

  client.print(request);

  if (client.println() == 0)
  {
    Serial.println(F("Failed to send request"));
    restartCounter++;
    return false;
  }

  return true;
}

boolean isStatusOk()
{
  char status[32] = {0};
  client.readBytesUntil('\n', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 200\r") != 0)
  {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    restartCounter++;
    return false;
  }
  return true;
}

boolean getDateFromHeader()
{
  char buf[5];
  if (client.find("Date: ") && client.readBytes(buf, 5) == 5)
  {
    int day = client.parseInt();    // day
    client.readBytes(buf, 1);       // discard
    client.readBytes(buf, 3);       // month
    int year = client.parseInt();   // year
    int hour = client.parseInt();   // hour
    int minute = client.parseInt(); // minute
    int second = client.parseInt(); // second
    int month;
    switch (buf[0])
    {
    case 'F':
      month = 2;
      break; // Feb
    case 'S':
      month = 9;
      break; // Sep
    case 'O':
      month = 10;
      break; // Oct
    case 'N':
      month = 11;
      break; // Nov
    case 'D':
      month = 12;
      break; // Dec
    default:
      if (buf[0] == 'J' && buf[1] == 'a')
      {
        month = 1; // Jan
      }
      else if (buf[0] == 'A' && buf[1] == 'p')
      {
        month = 4; // Apr
      }
      else
        switch (buf[2])
        {
        case 'r':
          month = 3;
          break; // Mar
        case 'y':
          month = 5;
          break; // May
        case 'n':
          month = 6;
          break; // Jun
        case 'l':
          month = 7;
          break; // Jul
        default: // add a default label here to avoid compiler warning
        case 'g':
          month = 8;
          break; // Aug
        }
    }
    serverTime = DateTime(year, month, day, hour, minute, second);
    return true;
  }
  return false;
}

boolean jumpToEndOfHeaders()
{
  if (!client.find("\r\n\r\n"))
  {
    Serial.println(F("Invalid response"));
    restartCounter++;
    return false;
  }
  return true;
}

boolean extractDeparturesFromResponse()
{
  numOfConnections = 0;
  StaticJsonBuffer<CAPACITY> jsonBuffer;
  JsonObject &response = jsonBuffer.parseObject(client);
  if (!response.success())
  {
    Serial.println(F("Parsing failed!"));
    restartCounter++;
    return false;
  }
  JsonArray &departures = response[F("departures")];
  Serial.print(F("Number of departures: "));
  Serial.println(sizeof(departures));
  minutesChanged = false;
  for (int i = 0; i < sizeof(departures) && numOfConnections < 2; i++)
  {
    const char *destination = departures[i]["destination"].as<char *>();
    if (isRelevantDestination(destination))
    {
      String departureTime = departures[i]["departureTime"].as<String>().substring(0, 10);
      TimeSpan timespanToDeparture((DateTime)departureTime.toInt() - serverTime);
      String lineNumber = departures[i]["label"].as<char *>();
      connections[numOfConnections].line = departures[i]["label"].as<char *>();
      connections[numOfConnections].destinationId = findStationIdByName(destination);
      connections[numOfConnections].departureTime = departureTime.toInt();
      if (connections[numOfConnections].minutesToDeparture == timespanToDeparture.minutes() && !minutesChanged)
      {
        minutesChanged = false;
      }
      else
      {
        minutesChanged = true;
      }
      Serial.print(F("minutesChanged: "));
      Serial.println(String(minutesChanged));
      connections[numOfConnections].minutesToDeparture = timespanToDeparture.minutes();
      numOfConnections++;
    }
  }
  Serial.print(F("Number of connections: "));
  Serial.println(numOfConnections);
  client.stop();
  return true;
}

uint16_t findStationIdByName(const char *str)
{
  for (int i = 0; i < sizeof(stationMap) / sizeof(stationMap[0]); i++)
  {
    if (strcmp(stationMap[i].name, str) == 0)
    {
      return stationMap[i].stationId;
    }
  }
  return 0;
}

void sendDeparturesViaBle()
{
  uint8_t *line1 = (uint8_t *)connections[0].line;
  uint16_t stationId1 = connections[0].destinationId;
  uint32_t departureTime1 = connections[0].departureTime;
  uint8_t timeTillDeparture1 = connections[0].minutesToDeparture;
  uint8_t *line2 = (uint8_t *)connections[1].line;
  uint16_t stationId2 = connections[1].destinationId;
  uint32_t departureTime2 = connections[1].departureTime;
  uint8_t timeTillDeparture2 = connections[1].minutesToDeparture;

  uint8_t payload[20];
  payload[0] = line1[0];
  payload[1] = line1[1];
  payload[2] = line1[2];
  payload[3] = stationId1;
  payload[4] = stationId1 >> 8;
  payload[5] = departureTime1;
  payload[6] = departureTime1 >> 8;
  payload[7] = departureTime1 >> 16;
  payload[8] = departureTime1 >> 24;
  payload[9] = timeTillDeparture1;
  payload[10] = line2[0];
  payload[11] = line2[1];
  payload[12] = line2[2];
  payload[13] = stationId2;
  payload[14] = stationId2 >> 8;
  payload[15] = departureTime2;
  payload[16] = departureTime2 >> 8;
  payload[17] = departureTime2 >> 16;
  payload[18] = departureTime2 >> 24;
  payload[19] = timeTillDeparture2;

  pTxCharacteristic->setValue(payload, 20);

  pTxCharacteristic->setValue((uint8_t *)payload, 20);
  pTxCharacteristic->notify();
}
