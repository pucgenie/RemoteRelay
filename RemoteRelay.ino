/*************************************************************************
 *
 * This file is part of the Remoterelay Arduino sketch.
 * Copyleft 2018 Nicolas Agius <nicolas.agius@lps-it.fr>
 * Copyleft 2022 Johannes Unger (just minor enhancements)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * ***********************************************************************/

// hardcoded pinging of 8.8.8.8 to save space and config overhead

// board manager: "Generic ESP8266 Board" https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/
// preferences additional: https://dl.espressif.com/dl/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiManager.h>         // See https://github.com/tzapu/WiFiManager for documentation
//#include <strings_en.h>

#include <EEPROM.h>

#include "RemoteRelay.h"
char buffer[BUF_SIZE];

#include "WebHelper.h"
#include "ledsignalling.h"

#include "syntacticsugar.h"

ESP8266WebServer server(80);
Logger logger = Logger();
struct ST_SETTINGS settings;
bool shouldSaveConfig = false;

#ifdef NUVOTON_AT_REPLIES
static ATReplies atreplies = ATReplies();
#endif

static uint8_t channels[] = {
    MODE_OFF
  , MODE_OFF
#ifdef FOUR_WAY_MODE
  , MODE_OFF
  , MODE_OFF
#endif
                      };

//assert sizeof(channels) <= 9: "print functions are restricted to one-digit channel count"

/**
 * Flash memory helpers 
 ********************************************************************************/

// CRC8 simple calculation
// Based on https://github.com/PaulStoffregen/OneWire/blob/master/OneWire.cpp
uint8_t crc8(const uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;

  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; --i) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) {
        crc ^= 0x8C;
      }
      inbyte >>= 1;
    }
  }
  return crc;
}

void saveSettings(struct ST_SETTINGS &p_settings) {
  // Use the last byte for CRC
  uint8_t theCRC = crc8((uint8_t*) &p_settings, sizeof(struct ST_SETTINGS));
  
  EEPROM.put(0, p_settings);
  /*
  for (int i = sizeof(buffer); i --> 0; ) {
    EEPROM.write(i, buffer[i]);
  }
  */
  EEPROM.put(sizeof(struct ST_SETTINGS), theCRC);
  EEPROM.commit();
}

int loadSettings(struct ST_SETTINGS &p_settings) {
  uint8_t const *_buffer = (uint8_t*) &p_settings;
  // Use the last byte for CRC
  
  EEPROM.get(0, p_settings);
  /*
  for (int i = sizeof(struct ST_SETTINGS); i --> 0; ) {
    _buffer[i] = uint8_t(EEPROM.read(i));
  }
  */

  // Check CRC
  if (crc8(_buffer, sizeof(struct ST_SETTINGS)) != uint8_t(EEPROM.read(sizeof(struct ST_SETTINGS)))) {
    logger.info(F("Bad CRC, loading default settings..."));
    setDefaultSettings(p_settings);
    return 0;
  }
  logger.setDebug(p_settings.debug);
  logger.setSerial(p_settings.serial);
  logger.info(F("Loaded settings from flash"));

  // Display loaded setting on debug
  getJSONSettings(buffer, BUF_SIZE);
  logger.debug(F("FLASH: %s"), buffer);
  return 1;
}

void setDefaultSettings(struct ST_SETTINGS& p_settings) {
  String(F(DEFAULT_LOGIN)).toCharArray(p_settings.login, AUTHBASIC_LEN_USERNAME-1);
  String(F(DEFAULT_PASSWORD)).toCharArray(p_settings.password, AUTHBASIC_LEN_PASSWORD-1);
  String(F("RemoteRelay")).toCharArray(p_settings.ssid, 64);
  p_settings.debug = false;
  p_settings.serial = false;
  led_scream(0b10101010);
}


/**
 * General helpers 
 ********************************************************************************/

void setChannel(uint8_t channel, uint8_t mode) {
  byte payload[4];
  
  // Header
  payload[0] = 0xA0;
  
  // Select the channel
  payload[1] = channel;
  
  // Set the mode
  //  * 0 = open (off)
  //  * 1 = close (on)
  payload[2] = mode;
  
  // Compute checksum
  payload[3] = payload[0] + payload[1] + payload[2];
  
  // Save status 
  channels[channel - 1] = mode;
  
  logger.info(F("Channel %.1i switched to %.3s"), channel, (mode == MODE_ON) ? "on" : "off");
  logger.debug(F("Sending payload %02X%02X%02X%02X"), payload[0], payload[1], payload[2], payload[3]);
  
  // Give some time to the watchdog
  ESP.wdtFeed();
  yield();
  
  // Send hex payload
  Serial.write(payload, sizeof(payload));
  
  if (settings.serial) {
    Serial.println(""); // Clear the line for log output
  }
}

void getJSONSettings(char p_buffer[], size_t bufSize) {
  //Generate JSON 
  snprintf(p_buffer, bufSize, String(F(R"=="==({"login":"%s","password":"<hidden>","debug":%.5s,"serial":%.5s}
)=="==")).c_str()
    , settings.login
    , bool2str(settings.debug)
    , bool2str(settings.serial)
  );
}

char* getJSONState(uint8_t channel) {
  //Generate JSON 
  snprintf(buffer, BUF_SIZE, String(F(R"=="==({"channel":%.1i,"mode":"%.3s"}
)=="==")).c_str()
    , channel
    , (channels[channel - 1] == MODE_ON) ? "on" : "off"
  );

  return buffer;
}

void setup()  {
  WiFiManager wifiManager;
  
  Serial.begin(115200);
  EEPROM.begin(512);
  logger.info(F("RemoteRelay version %s started."), VERSION);
  
  // Load settigns from flash
  if (!loadSettings(settings)) {
    //logger.info("Saving defaults in 10 seconds...");
    //delay(10000);
    //saveSettings(settings);

    // nop - don't need to save defaults because they can be restored anytime. Save write cycles.
  }

  // Be sure the relay are in the default state (off)
  for (uint8_t i = sizeof(channels); i > 0; ) {
    setChannel(i, channels[--i]);
  }

  // Configure custom parameters
  WiFiManagerParameter http_login("htlogin", "HTTP Login", settings.login, AUTHBASIC_LEN_USERNAME);
  WiFiManagerParameter http_password("htpassword", "HTTP Password", settings.password, AUTHBASIC_LEN_PASSWORD, "type='password'");
  WiFiManagerParameter http_ssid("ht2ssid", "AP mode SSID", settings.ssid, 64);
  wifiManager.setSaveConfigCallback([](){
    shouldSaveConfig = true;
  });
  wifiManager.addParameter(&http_login);
  wifiManager.addParameter(&http_password);
  wifiManager.addParameter(&http_ssid);
  
  // Connect to Wifi or ask for SSID
  wifiManager.autoConnect(settings.ssid);

  // Save new configuration set by captive portal
  if (shouldSaveConfig) {
    strncpy(settings.login, http_login.getValue(), AUTHBASIC_LEN_USERNAME);
    strncpy(settings.password, http_password.getValue(), AUTHBASIC_LEN_PASSWORD);
    strncpy(settings.ssid, http_ssid.getValue(), 64);

    logger.info(F("Saving new config from portal web page"));
    saveSettings(settings);
  }

  // Display local ip
  logger.info(F("Connected. IP address: %s"), WiFi.localIP().toString().c_str());
  
  setup_web_handlers(sizeof(channels));
  server.begin();
  
  logger.info(F("HTTP server started."));
}

void resetWiFiManager() {
  WiFiManager wifiManager;
  //reset saved settings
  wifiManager.resetSettings();
}

void loop() {
  server.handleClient();
#ifdef NUVOTON_AT_REPLIES
  // pretend to be an AT device here
  if (Serial.available()) {
    atreplies.handle_nuvoTon_comms(logger);
  }
#endif
}
