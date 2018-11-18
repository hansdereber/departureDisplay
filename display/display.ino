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

void setup()
{
  Serial.begin(115200);

  Serial.print("BLE_GATT_ATT_MTU_MAX: ");
  Serial.println(String(BLE_GATT_ATT_MTU_MAX));

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
    uint16_t stationId1;
    uint32_t departureTime1;
    uint8_t timeTillDeparture1;
    char line2[4];
    uint16_t stationId2;
    uint32_t departureTime2;
    uint8_t timeTillDeparture2;

    line1[0] = payload[0];
    line1[1] = payload[1];
    line1[2] = payload[2];
    line1[3] = '\0';
    stationId1 = (payload[4] << 8) | payload[3];
    departureTime1 = (payload[8] << 24) | (payload[7] << 16) | (payload[6] << 8) | payload[5];
    timeTillDeparture1 = payload[9];
    line2[0] = payload[10];
    line2[1] = payload[11];
    line2[2] = payload[12];
    line2[3] = '\0';
    stationId2 = (payload[14] << 8) | payload[13];
    departureTime2 = (payload[18] << 24) | (payload[17] << 16) | (payload[16] << 8) | payload[15];
    timeTillDeparture2 = payload[19];

    Serial.print("line1: ");
    Serial.println(line1);
    Serial.print("stationId1: ");
    Serial.println(String(stationId1));
    Serial.print("departureTime1: ");
    Serial.println(String(departureTime1));
    Serial.print("timeTillDeparture1: ");
    Serial.println(String(timeTillDeparture1));
    Serial.print("line2: ");
    Serial.println(String(line2));
    Serial.print("stationId2: ");
    Serial.println(String(stationId2));
    Serial.print("departureTime2: ");
    Serial.println(String(departureTime2));
    Serial.print("timeTillDeparture2: ");
    Serial.println(String(timeTillDeparture2));

    uart_svc.flush();
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