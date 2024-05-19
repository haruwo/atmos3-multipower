#include <M5AtomS3.h>
#include <Wire.h>
#include <WiFi.h>

#include "defer.h"
#include "local_ssid_define.h"

#define Serial USBSerial

#define DISPLAY_BRIGHTNESS 128

LGFX_Sprite Sprite(&AtomS3.Display);

enum WifiState
{
  WIFI_STATE_UNKNOWN,
  WIFI_STATE_HOME,
  WIFI_STATE_AWAY,
};

struct StateSet
{
  int v2;
  int pwr;
  WifiState wifi;

  bool operator==(const StateSet &rhs) const
  {
    return v2 == rhs.v2 && pwr == rhs.pwr && wifi == rhs.wifi;
  }

  bool operator!=(const StateSet &rhs) const
  {
    return !(*this == rhs);
  }
};

StateSet currentState{
    1,
    -1,
    WIFI_STATE_UNKNOWN,
};

StateSet currentDisplayState{
    1,
    -1,
    WIFI_STATE_UNKNOWN,
};

time_t lastDisplayUpdate = 0;

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

class V2SwitchClass
{
public:
  V2SwitchClass(int gpio) : _gpio(gpio)
  {
  }

  void begin()
  {
    pinMode(_gpio, OUTPUT);
  }

  void on()
  {
    digitalWrite(_gpio, HIGH);
  }

  void off()
  {
    digitalWrite(_gpio, LOW);
  }

private:
  int _gpio;
};

V2SwitchClass V2Switch(1);

void updateDisplay(const StateSet &state)
{
  Sprite.clear();
  Sprite.fillScreen(TFT_BLACK);

  Sprite.setFont(&fonts::FreeMono9pt7b);
  Sprite.setTextSize(1);

  // Power MultiPlexer
  Sprite.setCursor(6, 6);
  switch (state.pwr)
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
  switch (state.v2)
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
  switch (state.wifi)
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

void v2on()
{
  V2Switch.on();
  currentState.v2 = 1;
}

void v2off()
{
  V2Switch.off();
  currentState.v2 = 0;
}

void btnWather(void *pvParameters)
{
  while (1)
  {
    AtomS3.update();
    if (AtomS3.BtnA.wasClicked())
    {
      if (M5.Display.getBrightness() == 0)
      {
        Serial.println("Display on");
        lastDisplayUpdate = time(nullptr);
        M5.Display.powerSaveOff();
        M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
      }
      else if (currentState.v2 == 1)
      {
        Serial.println("Turn off Battery");
        v2off();
      }
      else
      {
        Serial.println("Turn on Battery");
        v2on();
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
    if (currentState.pwr != state)
    {
      currentState.pwr = state;
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

    const auto beforeState = currentState.wifi;
    currentState.wifi = found ? WIFI_STATE_HOME : WIFI_STATE_AWAY;
    if (beforeState != currentState.wifi)
    {
      Serial.printf("WiFi state changed: %d\n", currentState.wifi);
      if (currentState.wifi == WIFI_STATE_HOME)
      {
        // turn off Battery
        v2off();
      }
      else
      {
        // turn on Battery
        v2on();
      }
    }

    vTaskDelay(60 * 1000);
  }
}

void setup()
{
  AtomS3.begin(true);
  AtomS3.Display.setBrightness(DISPLAY_BRIGHTNESS);
  Serial.begin(115200);
  // I2C 400kbps
  Wire.begin(2, 1, 400UL * 1000UL);
  PowerMultiPlexer.begin();

  delay(1000); // wait for Serial Monitor

  Sprite.createSprite(128, 128);

  // set initial state to on
  xTaskCreate(btnWather, "btnWather", 4096, NULL, 1, NULL);
  xTaskCreate(powerStateWatcher, "powerStateWatcher", 4096, NULL, 1, NULL);
  xTaskCreate(wifiWatcher, "wifiWatcher", 4096, NULL, 1, NULL);

  updateDisplay(currentState);
  lastDisplayUpdate = time(nullptr);
}

void loop()
{
  if (currentDisplayState != currentState)
  {
    // update display
    const auto brightness = AtomS3.Display.getBrightness();
    if (brightness < DISPLAY_BRIGHTNESS)
    {
      Serial.println("Display on");
      M5.Display.powerSaveOff();
      M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
    }

    Serial.println("Display update");
    updateDisplay(currentState);
    lastDisplayUpdate = time(nullptr);
    currentDisplayState = currentState;
  }
  else if (time(nullptr) - lastDisplayUpdate > 60)
  {
    // turn off display if no update for 60 seconds
    const auto brightness = AtomS3.Display.getBrightness();
    if (brightness > 0)
    {
      Serial.println("Display off");
      M5.Display.setBrightness(0);
      M5.Display.powerSaveOn();
    }
  }
  delay(100);
}