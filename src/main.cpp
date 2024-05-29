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

enum PowerState
{
  POWER_STATE_UNKNOWN,
  POWER_STATE_ACC,
  POWER_STATE_BAT,
};

class StateSet
{
public:
  bool operator==(const StateSet &rhs) const
  {
    return v2 == rhs.v2 && pwr == rhs.pwr && wifi == rhs.wifi;
  }

  bool operator!=(const StateSet &rhs) const
  {
    return !(*this == rhs);
  }

  StateSet &operator=(const StateSet &rhs)
  {
    v2 = rhs.v2;
    pwr = rhs.pwr;
    wifi = rhs.wifi;
    lastUpdate = rhs.lastUpdate;
    return *this;
  }

  int getV2() const
  {
    return v2;
  }

  void setV2(const int v2)
  {
    this->v2 = v2;
    this->lastUpdate = time(nullptr);
  }

  PowerState getPwr() const
  {
    return pwr;
  }

  void setPwr(const PowerState pwr)
  {
    this->pwr = pwr;
    this->lastUpdate = time(nullptr);
  }

  WifiState getWifi() const
  {
    return wifi;
  }

  void setWifi(const WifiState wifi)
  {
    this->wifi = wifi;
    this->lastUpdate = time(nullptr);
  }

  time_t getLastUpdate() const
  {
    return lastUpdate;
  }

  time_t remainForShutdown() const
  {
    if (pwr == POWER_STATE_BAT && wifi == WIFI_STATE_HOME)
    {
      return lastUpdate + shutdownTimeSec - time(nullptr);
    }
    // max time
    return 0x7FFFFFFF;
  }

private:
  int v2 = -1;
  PowerState pwr = POWER_STATE_UNKNOWN;
  WifiState wifi = WIFI_STATE_UNKNOWN;
  time_t lastUpdate = 9;
  const time_t shutdownTimeSec = 60; // 1 minute
};

StateSet currentState;
StateSet currentDisplayState;

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
  Serial.printf("Update display: v2:%d pwr:%d wifi:%d\n", state.getV2(), state.getPwr(), state.getWifi());

  Sprite.clear();
  Sprite.fillScreen(TFT_BLACK);

  Sprite.setFont(&fonts::FreeMono9pt7b);
  Sprite.setTextSize(1);

  // Power MultiPlexer
  Sprite.setCursor(6, 6);
  switch (state.getPwr())
  {
  case POWER_STATE_ACC:
    Sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    Sprite.print("IN: ACC");
    break;
  case POWER_STATE_BAT:
    Sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    Sprite.print("IN: Bat");
    break;
  default:
    Sprite.setTextColor(TFT_RED, TFT_BLACK);
    Sprite.print("IN: ?");
    break;
  }

  // V2 Switch
  Sprite.setCursor(6, 30);
  switch (state.getV2())
  {
  case 1:
    Sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    Sprite.print("SW: Bat");
    break;
  case 0:
    Sprite.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    Sprite.print("SW: ACC");
    break;
  default:
    Sprite.setTextColor(TFT_RED, TFT_BLACK);
    Sprite.print("V2: ?");
    break;
  }

  // WiFi
  Sprite.setCursor(6, 54);
  switch (state.getWifi())
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

  // remaining time for shutdown
  Sprite.setCursor(6, 78);
  if (state.remainForShutdown() < 60)
  {
    Sprite.setTextColor(TFT_RED, TFT_BLACK);
    Sprite.print("Shutdown in ");
    Sprite.print(state.remainForShutdown());
    Sprite.print(" sec");
  }
  else if (state.remainForShutdown() < 60 * 60)
  {
    Sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    Sprite.print("Shutdown in ");
    Sprite.print(state.remainForShutdown() / 60);
    Sprite.print(" min");
  }
  else
  {
    Sprite.setTextColor(TFT_GREEN, TFT_BLACK);
    Sprite.print("Running");
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
  if (currentState.getV2() == 1)
  {
    return;
  }

  Serial.println("Turn on Battery");
  V2Switch.on();
  currentState.setV2(1);
}

void v2off()
{
  if (currentState.getV2() == 0)
  {
    return;
  }

  Serial.println("Turn off Battery");
  V2Switch.off();
  currentState.setV2(0);
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
      else if (currentState.getV2() == 1)
      {
        v2off();
      }
      else
      {
        v2on();
      }
    }
    vTaskDelay(100);
  }
}

void switchV2()
{
  if (currentState.getPwr() == POWER_STATE_BAT && currentState.getWifi() == WIFI_STATE_AWAY)
  {
    v2on();
  }
  else
  {
    v2off();
  }
  vTaskDelay(100);
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
    const PowerState pwr = state == 1 ? POWER_STATE_ACC : POWER_STATE_BAT;
    if (currentState.getPwr() != pwr)
    {
      currentState.setPwr(pwr);
      switchV2();
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

    const WifiState newState = found ? WIFI_STATE_HOME : WIFI_STATE_AWAY;
    if (currentState.getWifi() != newState)
    {
      Serial.printf("WiFi state changed: %d\n", newState);
      currentState.setWifi(newState);
      switchV2();
    }

    vTaskDelay(60 * 1000);
  }
}

void shutdownTimer(void *pvParameters)
{
  const time_t shutdownTimeSec = 60 * 5; // 5 minutes
  while (1)
  {
    if (currentState.remainForShutdown() <= 0)
    {
      Serial.println("Shutdown");
      M5.Power.deepSleep();
    }
    vTaskDelay(1000);
  }
}

void setup()
{
  AtomS3.begin(true);
  AtomS3.Display.setBrightness(DISPLAY_BRIGHTNESS);
  Serial.begin(115200);
  PowerMultiPlexer.begin();
  V2Switch.begin();

  delay(1000); // wait for Serial Monitor

  Sprite.createSprite(128, 128);

  // set initial state to on
  xTaskCreate(btnWather, "btnWather", 4096, NULL, 1, NULL);
  xTaskCreate(powerStateWatcher, "powerStateWatcher", 4096, NULL, 1, NULL);
  xTaskCreate(wifiWatcher, "wifiWatcher", 4096, NULL, 1, NULL);
  xTaskCreate(shutdownTimer, "shutdownTimer", 4096, NULL, 1, NULL);

  updateDisplay(currentState);
  lastDisplayUpdate = time(nullptr);
}

void loop()
{
  if (currentDisplayState != currentState || currentState.remainForShutdown() < 60)
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