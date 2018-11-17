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
}

int i = 0;

void loop()
{

  unsigned char image[120000];
  Paint paint(image, 400, 300); //width should be the multiple of 8
  paint.SetWidth(400);
  paint.SetHeight(300);
  paint.Clear(UNCOLORED);

  char buffer[100];

  sprintf(buffer, "Karlsplatz (Stachus) \n test blablabla \n tzestt");
  paint.DrawStringAt(0, 0, buffer, &Font24, COLORED);
  sprintf(buffer, "18");
  paint.DrawStringAt(0, 100, buffer, &Font24, COLORED);
  sprintf(buffer, "%d min", i);
  paint.DrawStringAt(280, 100, buffer, &Font24, COLORED);
  epd.SetPartialWindow(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight(), 2);
  epd.WaitUntilIdle();

  epd.DisplayFrameQuick();

  i++;
}
