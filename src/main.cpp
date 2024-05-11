#include <M5AtomS3.h>
#include <Wire.h>
#include <Y2Kb-USBRemoteI2C.h>

LGFX_Sprite Sprite(&AtomS3.Display);

int lastState = -1;

void updateDisplay(const int state)
{
  Sprite.clear();
  if (state == 0)
  {
    // Gray
    Sprite.setColor(0x80, 0x80, 0x80);
    Sprite.fillScreen();
  }
  else if (state == 1)
  {
    // Blue
    Sprite.setColor(0x00, 0x00, 0xFF);
    Sprite.fillScreen();
  }
  else if (state == -1)
  {
    // Red
    Sprite.setColor(0xFF, 0x00, 0x00);
    Sprite.fillScreen();
  }
  Sprite.pushSprite(0, 0);
}

void setup()
{
  AtomS3.begin(false);
  Serial.begin(115200);
  Wire1.begin();

  delay(1000); // wait for Serial Monitor

  USBRemoteI2C.begin();
  Sprite.createSprite(128, 128);
  lastState = USBRemoteI2C.read();
  updateDisplay(lastState);
  if (lastState < 0)
  {
    Serial1.println("Failed to read USB Remote I2C");
    delay(5000U);
    ESP.restart();
  }

  Serial.printf("Power state: %d\n", lastState);
}

void loop()
{
  AtomS3.update();

  if (AtomS3.BtnA.wasClicked())
  {
    lastState = (lastState == 0) ? 1 : 0;
    if (lastState == 0)
    {
      USBRemoteI2C.off();
    }
    else
    {
      USBRemoteI2C.on();
    }
    updateDisplay(lastState);
  }

  delay(50);
}