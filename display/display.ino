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
  Serial.print("[RX]: ");

  if (uart_svc.available())
  {
    sprintf(buffer, "receiving something %c", (char)uart_svc.read());
    displayString(buffer);
  }

  while (uart_svc.available())
  {
    Serial.print((char)uart_svc.read());
  }

  Serial.println();
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