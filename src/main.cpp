#include <M5AtomS3.h>
#include <Wire.h>
#include <WiFi.h>
#include <Y2Kb-USBRemoteI2C.h>

#include "defer.h"
#include "local_ssid_define.h"

LGFX_Sprite Sprite(&AtomS3.Display);

int v2LastState = -1;
int pwrLastState = -1;

enum WifiState
{
  WIFI_STATE_UNKNOWN,
  WIFI_STATE_HOME,
  WIFI_STATE_AWAY,
};

WifiState wifiState = WIFI_STATE_UNKNOWN;

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

void updateDisplay(const int p2State, const int pmpState, const WifiState wifiState)
{
  Sprite.clear();
  Sprite.fillScreen(TFT_BLACK);

  Sprite.setFont(&fonts::FreeMono9pt7b);
  Sprite.setTextSize(1);

  // Power MultiPlexer
  Sprite.setCursor(6, 6);
  switch (pmpState)
  {
  case 1:
    Sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    Sprite.print("IN: ACC");
    break;
  case 2:
    Sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    Sprite.print("IN: Bat");
    break;
  default:
    Sprite.setTextColor(TFT_RED, TFT_BLACK);
    Sprite.print("IN: ?");
    break;
  }

  // USB Remote I2C
  Sprite.setCursor(6, 30);
  switch (p2State)
  {
  case 1:
    Sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    Sprite.print("V2: ON");
    break;
  case 0:
    Sprite.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    Sprite.print("V2: OFF");
    break;
  default:
    Sprite.setTextColor(TFT_RED, TFT_BLACK);
    Sprite.print("V2: ?");
    break;
  }

  // WiFi
  Sprite.setCursor(6, 54);
  switch (wifiState)
  {
  case WIFI_STATE_HOME:
    Sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    Sprite.print("WF: HOME");
    break;
  case WIFI_STATE_AWAY:
    Sprite.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    Sprite.print("WF: AWAY");
    break;
  default:
    Sprite.setTextColor(TFT_RED, TFT_BLACK);
    Sprite.print("WF: ?");
    break;
  }

  Sprite.pushSprite(0, 0);
}

void onError(const char *msg)
{
  Sprite.clear();
  Sprite.setTextSize(2);
  Sprite.setTextColor(TFT_RED, TFT_BLACK);
  Sprite.setCursor(0, 0);
  Sprite.print(msg);
  Sprite.pushSprite(0, 0);
}

void btnWather(void *pvParameters)
{
  while (1)
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
    vTaskDelay(100);
  }
}

void powerStateWatcher(void *pvParameters)
{
  while (1)
  {
    int state = PowerMultiPlexer.input();
    if (state <= 0)
    {
      Serial1.println("Failed to read GPIO");
      vTaskDelay(1000);
      continue;
    }
    if (pwrLastState != state)
    {
      pwrLastState = state;
    }
    vTaskDelay(100);
  }
}

void i2cWatcher(void *pvParameters)
{
  while (1)
  {
    int state = USBRemoteI2C.read();
    if (state < 0)
    {
      Serial1.println("Failed to read USB Remote I2C");
      vTaskDelay(1000);
      continue;
    }
    if (v2LastState != state)
    {
      v2LastState = state;
    }
    vTaskDelay(100);
  }
}

void wifiWatcher(void *pvParameters)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  vTaskDelay(100);

  while (1)
  {
    Serial.println("Scanning WiFi");
    int n = WiFi.scanNetworks();
    defer { WiFi.scanDelete(); };

    bool found = false;
    if (n == 0)
    {
      Serial.println("No networks found");
    }
    else
    {
      Serial.print(n);
      Serial.println(" networks found");

      for (int i = 0; i < n; ++i)
      {
        if (WiFi.SSID(i).equals(WIFI_SSID))
        {
          found = true;
          break;
        }
      }
    }

    const auto beforeState = wifiState;
    wifiState = found ? WIFI_STATE_HOME : WIFI_STATE_AWAY;
    if (beforeState != wifiState)
    {
      Serial.printf("WiFi state changed: %d\n", wifiState);
      if (wifiState == WIFI_STATE_HOME)
      {
        // turn off Battery
        USBRemoteI2C.off();
        v2LastState = 0;
      }
      else
      {
        // turn on Battery
        USBRemoteI2C.on();
        v2LastState = 1;
      }
    }

    vTaskDelay(60 * 1000);
  }
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

  // set initial state to on
  USBRemoteI2C.on();
  USBRemoteI2C.updateInitialState(1);
  v2LastState = USBRemoteI2C.read();
  if (v2LastState < 0)
  {
    Serial1.println("Failed to read USB Remote I2C");
  }
  Serial.printf("Power state: %d\n", v2LastState);

  xTaskCreate(btnWather, "btnWather", 4096, NULL, 1, NULL);
  xTaskCreate(powerStateWatcher, "powerStateWatcher", 4096, NULL, 1, NULL);
  xTaskCreate(i2cWatcher, "i2cWatcher", 4096, NULL, 1, NULL);
  xTaskCreate(wifiWatcher, "wifiWatcher", 4096, NULL, 1, NULL);
}

void loop()
{
  updateDisplay(v2LastState, pwrLastState, wifiState);
  delay(100);
}