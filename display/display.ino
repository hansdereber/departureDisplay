/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include <SPI.h>
#include "epd4in2.h"
#include "epdpaint.h"

#define COLORED 0
#define UNCOLORED 1

Epd epd;

void setup()
{
  Serial.begin(115200);

  if (epd.Init() != 0)
  {
    Serial.print("e-Paper init failed");
    return;
  }

  /* This clears the SRAM of the e-paper display */
  epd.ClearFrame();
  epd.DisplayFrame();
}

int i = 0;
unsigned char image[16000];

void loop()
{
  Paint paint(image, 400, 300); //width should be the multiple of 8
  paint.SetWidth(400);
  paint.SetHeight(300);
  paint.Clear(UNCOLORED);

  char buffer[100];

  sprintf(buffer, "Karlsplatz (Sta...");
  paint.DrawStringAt(0, 0, buffer, &LiberationMedium, COLORED);
  sprintf(buffer, "18");
  paint.DrawStringAt(0, 35, buffer, &LiberationLarge, COLORED);
  if (i < 10)
  {
    sprintf(buffer, " %d", i);
  }
  else
  {
    sprintf(buffer, "%d", i);
  }
  paint.DrawStringAt(157, 35, buffer, &LiberationLarge, COLORED);
  sprintf(buffer, "min");
  paint.DrawStringAt(260, 35, buffer, &LiberationLarge, COLORED);
  sprintf(buffer, "Karlsplatz (Sta...");
  paint.DrawStringAt(0, 180, buffer, &LiberationMedium, COLORED);
  sprintf(buffer, "18");
  paint.DrawStringAt(0, 215, buffer, &LiberationLarge, COLORED);
  if (i < 10)
  {
    sprintf(buffer, " %d", i);
  }
  else
  {
    sprintf(buffer, "%d", i);
  }
  paint.DrawStringAt(157, 215, buffer, &LiberationLarge, COLORED);
  sprintf(buffer, "min");
  paint.DrawStringAt(260, 215, buffer, &LiberationLarge, COLORED);
  epd.SetPartialWindow(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight(), 2);
  epd.DisplayFrameQuick();
  epd.WaitUntilIdle();
  epd.Sleep();
  i++;
  delay(2000);

  epd.Reset();
  epd.Init();
}
