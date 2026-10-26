// Minimale Basis: OTA-Kern + Web-Upload. WLAN-/Webmodul, Regelung und Konfiguration folgen in eigenen Paketen.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <OneWire.h>
#include <Adafruit_EMC2101.h>
#include "ConfigManager.h"
#include "BoardPins.h"
#include "FirmwareInfo.h"
#include "ICoolingGuard.h"
#include "WifiConnection.h"
#include "update/BundleUpdater.h"
#include "update/EspUpdatePlatform.h"
#include "update/UpdateManager.h"
#include "update/WebUpdater.h"
#include "network/MqttManager.h"
#include "web/WebUi.h"

static NullCoolingGuard coolingGuard;  // Platzhalter bis zum Regelungs-Paket
static EspUpdatePlatform updatePlatform;
static UpdateManager updates;
static BundleUpdater updateBundle;
static ESP8266WebServer server(80);
static WebUpdater webUpdater;
static WebUi webUi;
static ConfigManager configManager;
static WifiConnection wifiConnection;
static MqttManager mqtt;
static OneWire oneWireBus(BoardPins::ONE_WIRE);
static Adafruit_EMC2101 emc2101;
static bool emc2101Ready = false;

static void initializeHardware() {
  oneWireBus.reset_search();

  pinMode(BoardPins::ONBOARD_LED, OUTPUT);
  digitalWrite(BoardPins::ONBOARD_LED,
               BoardPins::ONBOARD_LED_ACTIVE_LOW ? HIGH : LOW);

  // Tach inputs use their separately wired 3.3 V pull-ups.
  pinMode(BoardPins::FAN1_TACH, INPUT);
  pinMode(BoardPins::FAN2_TACH, INPUT);

  Wire.begin(BoardPins::I2C_SDA, BoardPins::I2C_SCL);
  emc2101Ready = emc2101.begin(EMC2101_I2CADDR_DEFAULT, &Wire);
  if (emc2101Ready) {
    Serial.println("EMC2101 erkannt");
  } else {
    Serial.println("WARN: EMC2101 nicht erkannt; Firmware laeuft ohne PWM-Ausgabe weiter");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nVENTIQ %s v%s\n", FirmwareInfo::hardware(), FirmwareInfo::versionString());
  initializeHardware();

  const bool fileSystemReady = LittleFS.begin();
  const ConfigStatus configStatus = configManager.begin(fileSystemReady);
  Serial.printf("Konfiguration: %s\n", configStatusName(configStatus));

  wifiConnection.begin(configManager.get().wifi, configManager.ready(), millis());

  updates.begin(&updatePlatform, &coolingGuard, FirmwareInfo::marker());
  if (!updateBundle.begin(&updatePlatform, &updates, FirmwareInfo::marker(), fileSystemReady) ||
      !BundleUpdater::recoverAtBoot(fileSystemReady, FirmwareInfo::marker())) {
    Serial.println("WARN: OTA-Bundle-Wiederherstellung fehlgeschlagen");
  }
  webUpdater.begin(server, updates, updateBundle);
  webUi.begin(server, fileSystemReady, configManager);
  server.begin();
}

void loop() {
  server.handleClient();
  wifiConnection.update(configManager.get().wifi, configManager.ready(), millis());
  mqtt.update(configManager.get().mqtt, configManager.ready(), millis());
  updates.tick();
}
