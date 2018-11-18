#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "RTClib.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "SSD1306.h"
#include <BLE2902.h>

// constants
const char *SSID PROGMEM = "affenstunk";
const char *PASSWORD PROGMEM = "Schluss3ndlichkeit";
const char *HOST PROGMEM = "www.mvg.de";
const char *URL PROGMEM = "fahrinfo/api/departure/1109?footway=0";
const char *API_KEY PROGMEM = "5af1beca494712ed38d313714d4caff6";
const char *SERVICE_UUID PROGMEM = "2e80f571-d90f-4a75-818b-e90434e5ffaa";
const char *DISPLAY_TEXT_UUID PROGMEM = "c623addf-a88a-4d26-b78e-15baf8195cdd";
const char *BLE_DEVICE_NAME PROGMEM = "departures central";
const char *PERIPHERAL_ADDRESS PROGMEM = "e9:fa:f5:f5:4f:ba";
const size_t CAPACITY PROGMEM =
    JSON_ARRAY_SIZE(2) +
    JSON_ARRAY_SIZE(20) +
    JSON_OBJECT_SIZE(2) +
    2 * JSON_OBJECT_SIZE(6) +
    20 * JSON_OBJECT_SIZE(8) +
    0; //3000

const String request PROGMEM = String(F("GET https://")) + HOST + "/" + URL + " HTTP/1.1\r\n" +
                               "Host: " + HOST + "\r\n" +
                               "X-MVG-Authorization-Key: " + API_KEY + "\r\n" +
                               "Connection: close\r\n\r\n";

struct Connection
{
  const char *lineNumber;
  const char *destination;
  TimeSpan timespanToDeparture;
};

// global variables
WiFiClientSecure client;
BLECharacteristic *pDisplayText;
DateTime serverTime;
struct Connection connections[15];
int numOfConnections;
SSD1306 display(0x3c, 5, 4);
char displayText[64];
static BLEAddress *pPeripheralAddress;
BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic *pTxCharacteristic;
BLECharacteristic *pRxCharacteristic;
static boolean doConnect = false;
static boolean connected = false;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    Serial.println(advertisedDevice.getAddress().toString().c_str());
    if (advertisedDevice.getAddress().toString() == PERIPHERAL_ADDRESS)
    {
      Serial.print("Found our device!  address: ");
      advertisedDevice.getScan()->stop();
      pPeripheralAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  }
};

void setup()
{
  Serial.begin(115200);
  initDisplay();
  initBluetoothLowEnergy();
  Serial.println(F("Reserved space for JSON Object: "));
  Serial.println(String(CAPACITY));
}

boolean isRelevantDestination(String destination)
{
  return (destination == F("Ostbahnhof") ||
          destination == F("Messestadt West") ||
          destination == F("Karlsplatz (Stachus)") ||
          destination == F("Trudering Bf.") ||
          destination == F("Giesing Bf.") ||
          destination == F("Ostfriedhof"));
}

void initDisplay()
{
  display.init();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
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

  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  pService->start();
}

void loop()
{
  if (doConnect == true)
  {
    if (connectToServer(*pPeripheralAddress))
    {
      Serial.println("We are now connected to the BLE Server.");
      connected = true;
    }
    else
    {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  delay(2000);
  size_t size = 60;
  Serial.println("setting value to:");
  Serial.print("ESP_GATT_MAX_ATTR_LEN: ");
  Serial.println(String(ESP_GATT_MAX_ATTR_LEN));


  uint8_t* line = (uint8_t*) "123";
  uint16_t stationId = 3042;
  uint8_t timeTillDeparture = 7;
  uint32_t departureTime = 1542570180;

	uint8_t payload[20];
  payload[0]=line[0];
  payload[1]=line[1];
  payload[2]=line[2];
	payload[3]=stationId;
	payload[4]=stationId>>8;
  payload[5]=departureTime;
  payload[6]=departureTime>>8;
  payload[7]=departureTime>>16;
  payload[8]=departureTime>>24;
  payload[9]=timeTillDeparture;
  payload[10]=line[0];
  payload[11]=line[1];
  payload[12]=line[2];
	payload[13]=stationId;
	payload[14]=stationId>>8;
  payload[15]=departureTime;
  payload[16]=departureTime>>8;
  payload[17]=departureTime>>16;
  payload[18]=departureTime>>24;
  payload[19]=timeTillDeparture;

	pTxCharacteristic->setValue(payload, 20);



  pTxCharacteristic->setValue((uint8_t*) payload, size);
  pTxCharacteristic->notify();
  delay(2000);

  /*  
  display.setPixel(127, 63);
  display.display();
  checkAndConnectToWifi();
  Serial.println(F("Requesting departures"));
  if (getDepartures())
  {
    Serial.println(F("Drawing departures"));
    createDisplayText();
    drawTextOnDisplay();
    sendDeparturesViaBle();
    delay(10000);
  }
  else
  {
    display.drawLine(0, 63, 127, 0);
    display.drawLine(0, 0, 127, 63);
    display.display();
    delay(60000);
  }
  */
}

bool connectToServer(BLEAddress pAddress)
{
  Serial.print("Forming a connection to ");
  Serial.println(pAddress.toString().c_str());
  BLEClient *pClient = BLEDevice::createClient();
  Serial.println(" - Created client");
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");
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
    return false;
  }

  client.print(request);

  if (client.println() == 0)
  {
    Serial.println(F("Failed to send request"));
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
    Serial.print("Date found with minute: ");
    Serial.println(String(minute));
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
    return false;
  }
  JsonArray &departures = response[F("departures")];
  Serial.print(F("Number of departures: "));
  Serial.println(sizeof(departures));
  for (int i = 0; i < sizeof(departures); i++)
  {
    const char *destination = departures[i][F("destination")].as<char *>();
    if (isRelevantDestination(destination))
    {
      String departureTime = departures[i][F("departureTime")].as<String>().substring(0, 10);
      DateTime departureDateTime(departureTime.toInt());
      String lineNumber = departures[i][F("label")].as<char *>();
      connections[numOfConnections].lineNumber = departures[i][F("label")].as<char *>();
      connections[numOfConnections].destination = destination;
      connections[numOfConnections].timespanToDeparture = departureDateTime - serverTime;
      numOfConnections++;
    }
  }
  Serial.print(F("Number of connections: "));
  Serial.println(numOfConnections);
  client.stop();
  return true;
}

void createDisplayText()
{
  displayText[0] = '\0';
  for (int i = 0; i < numOfConnections && i < 2; i++)
  {
    char row[32];
    sprintf(row, "%s: %d min\n", connections[i].lineNumber, connections[i].timespanToDeparture.minutes());
    strcat(displayText, row);
  }
  Serial.println(F("------------"));
  Serial.println(displayText);
  Serial.println(F("------------"));
}

void drawTextOnDisplay()
{
  display.clear();
  display.drawString(0, 0, displayText);
  display.display();
}

void sendDeparturesViaBle()
{
  pDisplayText->setValue(displayText);
  pDisplayText->notify();
}
