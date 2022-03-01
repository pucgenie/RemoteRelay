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
// AT+RESTORE could change from AUTO_MODE to AP_REQUESTED

// board manager: "Generic ESP8266 Board" https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/
// preferences additional: https://dl.espressif.com/dl/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json

#include <EEPROM.h>

#include "RemoteRelay.h"
char buffer[BUF_SIZE];

#include "WebHelper.h"
#include "ledsignalling.h"

#include "syntacticsugar.h"

//ESP8266WebServer server(80);
// contained in WiFiManager.server->

struct ST_SETTINGS settings;
Logger logger = Logger();
bool shouldSaveConfig = false;
MyLoopState myLoopState = AFTER_SETUP;
WiFiManager wifiManager;

#ifndef DISABLE_NUVOTON_AT_REPLIES
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
  // TODO: make use of wearlevel_mark, shift save position
  EEPROM.put(0, p_settings);
  /*
  for (int i = sizeof(buffer); i --> 0; ) {
    EEPROM.write(i, buffer[i]);
  }
  */
  EEPROM.put(sizeof(struct ST_SETTINGS), theCRC);
  EEPROM.commit();
}

bool loadSettings(struct ST_SETTINGS &p_settings) {
  // Use the last byte for CRC
  
  EEPROM.get(0, p_settings);
  uint8_t const *_buffer = (uint8_t*) &p_settings;
  /*
  for (int i = sizeof(struct ST_SETTINGS); i --> 0; ) {
    _buffer[i] = uint8_t(EEPROM.read(i));
  }
  */

  // Check CRC
  bool validCRC = (crc8(_buffer, sizeof(struct ST_SETTINGS)) == uint8_t(EEPROM.read(sizeof(struct ST_SETTINGS))));
  if (!validCRC) {
    logger.info(PSTR("Bad CRC, loading default settings..."));
    setDefaultSettings(p_settings);
  }
  logger.setSerial(p_settings.flags.serial);
  logger.info(PSTR("Loaded settings from flash"));

  // Display loaded setting on debug
  if (settings.flags.debug) {
    getJSONSettings(buffer, BUF_SIZE);
    logger.debug(PSTR("FLASH: %s"), buffer);
  }
  return validCRC;
}

void setDefaultSettings(struct ST_SETTINGS& p_settings) {
  strncpy_P(p_settings.login, PSTR(DEFAULT_LOGIN), AUTHBASIC_LEN_USERNAME);
  strncpy_P(p_settings.password, PSTR(DEFAULT_PASSWORD), AUTHBASIC_LEN_PASSWORD);
  strncpy_P(p_settings.ssid, PSTR("RemoteRelay"), 64);
  p_settings.flags.wearlevel_mark = 0b11;
  p_settings.flags.debug = false;
  p_settings.flags.serial = false;
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
  
  logger.info(PSTR("Channel %.1i switched to %.3s"), channel, (mode == MODE_ON) ? "on" : "off");
  logger.debug(PSTR("Sending payload %02X%02X%02X%02X"), payload[0], payload[1], payload[2], payload[3]);
  
  // Give some time to the watchdog
  ESP.wdtFeed();
  yield();
  
  // Send hex payload
  Serial.write(payload, sizeof(payload));
  
  if (settings.flags.serial) {
    Serial.println(""); // Clear the line for log output
  }
}

void getJSONSettings(char p_buffer[], size_t bufSize) {
  //Generate JSON 
  snprintf_P(p_buffer, bufSize, PSTR(R"=="==({"login":"%s","password":"<hidden>","debug":%.5s,"serial":%.5s,"webservice":%.5s}
)=="==")
    , settings.login
    , bool2str(settings.flags.debug)
    , bool2str(settings.flags.serial)
    , bool2str(settings.flags.webservice)
  );
}

void getJSONState(uint8_t channel, char p_buffer[], size_t bufSize) {
  //Generate JSON 
  snprintf_P(p_buffer, bufSize, PSTR(R"=="==({"channel":%.1i,"mode":"%.3s"}
)=="==")
    , channel
    , (channels[channel - 1] == MODE_ON) ? "on" : "off"
  );
}

void configModeCallback (WiFiManager *myWiFiManager) {
  
}

void setup()  {
  Serial.begin(115200);
  EEPROM.begin(512);
  logger.info(PSTR("RemoteRelay version %s started."), VERSION);
  
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
}

void loop() {
  switch (myLoopState) {
    case SHUTDOWN_REQUESTED:
      myLoopState = SHUTDOWN_HALT;
      delay(3000);
    break;
    case SHUTDOWN_HALT:
      logger.info(PSTR("Restarting..."));
    
      ESP.restart();
    break;
    case WEB_REQUESTED:
      // Display local ip
      logger.info(PSTR("Connected. IP address: %s"), WiFi.localIP().toString().c_str());
      
      setup_web_handlers(sizeof(channels));
      /* wifiManager handles server
      server.begin();
      */
      
      logger.info(PSTR("HTTP server started."));
    break;
    case STA_REQUESTED: {
      WiFi.mode(WIFI_STA);
    
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
  
      wifiManager.setConfigPortalBlocking(false);
      wifiManager.setRemoveDuplicateAPs(false);
      wifiManager.setAPCallback(configModeCallback);
      // Connect to Wifi or ask for SSID
      bool res = wifiManager.autoConnect(settings.ssid);
    
      /*
      wifiManager.setConfigPortalTimeout(timeout);
      wifiManager.startConfigPortal(settings.ssid);
      wifiManager.startWebPortal();
      */
    }
    break;
    case AUTO_MODE:
    case STA_WEB: // fall-through
      /* now handled by wifiManager portal server service
      server.handleClient();
      */
    break;
    case EEPROM_DESTROY_CRC: {
      struct ST_SETTINGS tmp_settings;
      if (!loadSettings(tmp_settings)) {
        // it already is incorrect - nop.
    break;
      }
      uint8_t crc = EEPROM.read(sizeof(struct ST_SETTINGS));
      // what a coincidence
      if (crc == 0) {
        myLoopState = ERASE_EEPROM;
    break;
      }
      for (int i = 8; i --> 0; ) {
        
      }
      //EEPROM.write(sizeof(struct ST_SETTINGS), );
    }
    break;
    case ERASE_EEPROM:
      // spi_flash_geometry.h, FLASH_SECTOR_SIZE 0x1000
    break;
    default:
      logger.info(PSTR("WTF"));
    break;
  }

#ifndef DISABLE_NUVOTON_AT_REPLIES
  // pretend to be an AT device here
  if (Serial.available()) {
    atreplies.handle_nuvoTon_comms(logger);
  }
#endif
}
