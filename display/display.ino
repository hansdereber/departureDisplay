#include <SPI.h>
#include "epd4in2.h"
#include "epdpaint.h"

#define COLORED 0
#define UNCOLORED 1

Epd epd;
unsigned char image[16000];

void setup()
{
  Serial.begin(115200);

  if (epd.Init() != 0)
  {
    Serial.print("e-Paper init failed");
    return;
  }

  displayString("test");
}

void loop()
{
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