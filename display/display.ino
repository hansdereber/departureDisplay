#include <bluefruit.h>
#include "epd4in2.h"
#include "epdpaint.h"
#include <SPI.h>

#define COLORED 0
#define UNCOLORED 1

Epd epd;
unsigned char image[16000];
char buffer[128];

BLEClientUart clientUart;

struct Connection
{
  char line[4];
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
struct Map stationMap[6];

void setup()
{
  stationMap[0].stationId = 1;
  stationMap[0].name = "Karlsplatz";
  stationMap[1].stationId = 5;
  stationMap[1].name = "Ostbahnhof";
  stationMap[2].stationId = 1250;
  stationMap[2].name = "Messestadt West";
  stationMap[3].stationId = 920;
  stationMap[3].name = "Trudering Bf.";
  stationMap[4].stationId = 1110;
  stationMap[4].name = "Giesing Bf.";
  stationMap[5].stationId = 1105;
  stationMap[5].name = "Ostfriedhof";

  Serial.begin(115200);

  if (epd.Init() != 0)
  {
    Serial.print("e-Paper init failed");
    return;
  }

  displayString("");

  Bluefruit.autoConnLed(false);
  Bluefruit.begin(1, 0);
  Bluefruit.setTxPower(4);
  Bluefruit.setName("display");
  Bluefruit.setConnectCallback(connectCallback);
  Bluefruit.setDisconnectCallback(disconnectCallback);

  clientUart.begin();
  clientUart.setRxCallback(bleUartRxCallback);

  startAdv();
}

void connectCallback(uint16_t conn_handle)
{
  displayString("connected");

  if (clientUart.discover(conn_handle))
  {
    Serial.println("Found it");
    Serial.println("Enable TXD's notify");
    clientUart.enableTXD();
    Serial.println("Ready to receive from peripheral");
  }
  else
  {
    Serial.println("Found NONE");
    Bluefruit.Central.disconnect(conn_handle);
  }
}

void bleUartRxCallback(BLEClientUart &uart_svc)
{
  if (uart_svc.available())
  {
    uint8_t payload[20];
    uart_svc.read(payload, 20);

    char line1[4];
    uint16_t destinationId1;
    uint32_t departureTime1;
    uint8_t minutesToDeparture1;
    char line2[4];
    uint16_t destinationId2;
    uint32_t departureTime2;
    uint8_t minutesToDeparture2;

    connections[0].line[0] = line1[0] = payload[0];
    connections[0].line[1] = payload[1];
    connections[0].line[2] = payload[2];
    connections[0].line[3] = '\0';
    connections[0].destinationId = (payload[4] << 8) | payload[3];
    connections[0].departureTime = (payload[8] << 24) | (payload[7] << 16) | (payload[6] << 8) | payload[5];
    connections[0].minutesToDeparture = payload[9];
    connections[1].line[0] = payload[10];
    connections[1].line[1] = payload[11];
    connections[1].line[2] = payload[12];
    connections[1].line[3] = '\0';
    connections[1].destinationId = (payload[14] << 8) | payload[13];
    connections[1].departureTime = (payload[18] << 24) | (payload[17] << 16) | (payload[16] << 8) | payload[15];
    connections[1].minutesToDeparture = payload[19];

    Serial.print("line1: ");
    Serial.println(connections[0].line);
    Serial.print("destinationId1: ");
    Serial.println(String(connections[0].destinationId));
    Serial.print("departureTime1: ");
    Serial.println(String(connections[0].departureTime));
    Serial.print("minutesToDeparture1: ");
    Serial.println(String(connections[0].minutesToDeparture));
    Serial.print("line2: ");
    Serial.println(connections[1].line);
    Serial.print("destinationId2: ");
    Serial.println(String(connections[1].destinationId));
    Serial.print("departureTime2: ");
    Serial.println(String(connections[1].departureTime));
    Serial.print("minutesToDeparture2: ");
    Serial.println(String(connections[1].minutesToDeparture));

    uart_svc.flush();
    displayConnections();
  }
}

void disconnectCallback(uint16_t conn_handle, uint8_t reason)
{
  displayString("Not connected");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
  Bluefruit.Advertising.start(0);             // 0 = Don't stop advertising after n seconds
}

void displayString(const char *text)
{
  return;
  Paint paint(image, 400, 300);
  paint.SetWidth(400);
  paint.SetHeight(300);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, text, &LiberationMedium, COLORED);
  displayFrameQuick(paint);
}

void displayConnections()
{
  Paint paint(image, 400, 300); //width should be the multiple of 8
  paint.SetWidth(400);
  paint.SetHeight(300);
  paint.Clear(UNCOLORED);

  paint.DrawStringAt(0, 0, findStationNameById(connections[0].destinationId), &LiberationMedium, COLORED);
  paint.DrawStringAt(0, 35, connections[0].line, &LiberationLarge, COLORED);
  sprintf(buffer, "%d", connections[0].minutesToDeparture);
  paint.DrawStringAt(175, 35, buffer, &LiberationLarge, COLORED);
  sprintf(buffer, "min");
  paint.DrawStringAt(275, 35, buffer, &LiberationLarge, COLORED);
  paint.DrawStringAt(0, 180, findStationNameById(connections[1].destinationId), &LiberationMedium, COLORED);
  paint.DrawStringAt(0, 215, connections[1].line, &LiberationLarge, COLORED);
  sprintf(buffer, "%d", connections[1].minutesToDeparture);
  paint.DrawStringAt(175, 215, buffer, &LiberationLarge, COLORED);
  sprintf(buffer, "min");
  paint.DrawStringAt(275, 215, buffer, &LiberationLarge, COLORED);
  displayFrameQuick(paint);
}

const char* findStationNameById(uint16_t id)
{
  for (int i = 0; i < sizeof(stationMap) / sizeof(stationMap[0]); i++)
  {
    if (stationMap[i].stationId == id)
    {
      return stationMap[i].name;
    }
  }
  return "---";
}

void displayFrameQuick(Paint paint)
{
  wake();
  epd.SetPartialWindow(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight(), 2);
  epd.DisplayFrameQuick();
  sleep();
}

void wake()
{
  epd.Reset();
  epd.Init();
}

void sleep()
{
  epd.WaitUntilIdle();
  epd.Sleep();
}

void loop() {}