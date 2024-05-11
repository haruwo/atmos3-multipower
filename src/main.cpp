#include <M5AtomS3.h>
#include <Wire.h>
#include <Y2Kb-USBRemoteI2C.h>

LGFX_Sprite Sprite(&AtomS3.Display);

int v2LastState = -1;

class PowerMultiPlexerClass
{
public:
  PowerMultiPlexerClass(int gpio) : _gpio(gpio)
  {
  }

  void begin()
  {
    pinMode(_gpio, INPUT);
  }

  // 1: input1, 2: input2, 0: error
  int input()
  {
    int cur = digitalRead(_gpio);
    if (cur == HIGH)
    {
      return 2;
    }
    else // LOW
    {
      return 1;
    }
  }

private:
  int _gpio;
};

PowerMultiPlexerClass PowerMultiPlexer(38);

void updateDisplay(const int p2State, const int pmpState)
{
  Sprite.clear();

  // Power MultiPlexer
  if (pmpState == 1)
  {
    // Green
    Sprite.setColor(0x00, 0xFF, 0x00);
  }
  else if (pmpState == 2)
  {
    // Yellow
    Sprite.setColor(0xFF, 0xFF, 0x00);
  }
  else
  {
    // Red
    Sprite.setColor(0xFF, 0x00, 0x00);
  }
  Sprite.fillRect(0, 0, 128, 64);
  Sprite.setTextSize(3);
  Sprite.setCursor(0, 0);
  Sprite.print("PMP");

  // V2 State
  if (p2State == 0)
  {
    // Gray
    Sprite.setColor(0x80, 0x80, 0x80);
  }
  else if (p2State == 1)
  {
    // Blue
    Sprite.setColor(0x00, 0x00, 0xFF);
  }
  else
  {
    // Red
    Sprite.setColor(0xFF, 0x00, 0x00);
  }
  Sprite.fillRect(0, 64, 128, 64);
  Sprite.setTextSize(3);
  Sprite.setCursor(0, 64);
  Sprite.print("V2");

  // write center line
  // Black
  Sprite.setColor(0, 0, 0);
  Sprite.drawLine(0, 64, 128, 64);

  Sprite.pushSprite(0, 0);
}

void setup()
{
  AtomS3.begin(true);
  Serial.begin(115200);
  Wire.begin(2, 1, 1000000UL); // I2C1 (SCL, SDA, frequency
  USBRemoteI2C.begin();
  PowerMultiPlexer.begin();

  delay(1000); // wait for Serial Monitor

  Sprite.createSprite(128, 128);
  v2LastState = USBRemoteI2C.read();
  updateDisplay(v2LastState, PowerMultiPlexer.input());
  if (v2LastState < 0)
  {
    Serial1.println("Failed to read USB Remote I2C");
    delay(5000U);
    ESP.restart();
  }

  Serial.printf("Power state: %d\n", v2LastState);
}

void loop()
{
  AtomS3.update();

  if (AtomS3.BtnA.wasClicked())
  {
    if (v2LastState == 1)
    {
      USBRemoteI2C.off();
      v2LastState = 0;
    }
    else
    {
      USBRemoteI2C.on();
      v2LastState = 1;
    }
  }
  updateDisplay(v2LastState, PowerMultiPlexer.input());

  delay(100);
}