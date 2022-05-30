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
// AT+RESTORE could change from STA_{WEB,LITE} to AP_REQUESTED

// board manager: "Generic ESP8266 Board" https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/
// preferences additional: https://dl.espressif.com/dl/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json

// To control global EEPROM access (begin etc.)
#include "EEPROM.h"

// To detect Internet presence, more or less.
#include <AsyncPing.h>

#include "RemoteRelay.h"
char buffer[BUF_SIZE];

#include "WebHelper.h"
#include "ledsignalling.h"

#include "syntacticsugar.h"

//ESP8266WebServer server(80);
// contained in wifiManager.server->

RemoteRelaySettings settings;
Logger logger;
bool shouldSaveConfig = false;
MyLoopState myLoopState = AFTER_SETUP;
MyWiFiState myWiFiState = MYWIFI_OFF;
MyWebState myWebState = WEB_DISABLED;
MyPingState myPingState = PING_NONE;
/**
 * WiFiManagerParameters can't be removed so deleting the whole object is necessary.
**/
/**
 * How to deconstruct and re-initialize: https://stackoverflow.com/a/2166155/2714781
wifiManager.~WiFiManager();
new(&wifiManager) WiFiManager();
**/
WiFiManager wifiManager;
static AsyncPing ping;
static const __FlashStringHelper *serial_response_next = NULL;
// TODO: should be configurable
static IPAddress isp_endpoint(8,8,8,8);

#ifndef DISABLE_NUVOTON_AT_REPLIES
static ATReplies atreplies = ATReplies();
#endif

static int8_t channels[] = {
    MODE_OFF
  , MODE_OFF
#ifdef FOUR_WAY_MODE
  , MODE_OFF
  , MODE_OFF
#endif
};

static uint16_t settings_offset = 0;

/**
 * Flash memory helpers 
 ********************************************************************************/

//void setDefaultSettings(RemoteRelaySettings& p_settings)

/**
 * General helpers 
 ********************************************************************************/
struct RSTM32Payload {
  uint8_t header   :8 {0xA0};
  uint8_t channel  :8;
  RSTM32Mode mode  :8;
  uint8_t checksum :8;
};

void setChannel(uint8_t channel, RSTM32Mode mode) {
  struct RSTM32Payload payload;
  
  payload.channel = channel;

  payload.mode = mode;
  
  // Compute checksum
  payload.checksum = payload.header + payload.channel + ((int) payload.mode);
  
  static_assert(sizeof(channels) <= 9, "print functions are restricted to one-digit channel count");
  // Save status 
  channels[channel - 1] = mode;
  
  logger.info(F("{'channel': %.1i, 'state': '%.3s'}"), channel, (mode == MODE_ON) ? "on" : "off");
  logger.debug(F("{'payload': '%02X%02X%02X%02X'}"), payload[0], payload[1], payload[2], payload[3]);
  
  // Give some time to the watchdog
  ESP.wdtFeed();
  yield();
  
  // Send hex payload
  Serial.write((const uint8_t *) &payload, sizeof(payload));
  
  if (settings.flags.serial) {
    Serial.println(""); // Clear the line for log output
  }
}

size_t getJSONSettings(char p_buffer[], size_t bufSize) {
  //Generate JSON 
  size_t snstatus = snprintf_P(p_buffer, bufSize, PSTR(R"=="==({"login":"%s","debug":%.5s,"serial":%.5s,"webservice":%.5s,"wifimanager_portal":%.5s}
)=="==")
    , settings.login
    , bool2str(settings.flags.debug)
    , bool2str(settings.flags.serial)
    , bool2str(settings.flags.webservice)
    , bool2str(settings.flags.wifimanager_portal)
  );
  assert(snstatus > 0 && snstatus < bufSize);
  return snstatus;
}

size_t getJSONState(uint8_t channel, char p_buffer[], size_t bufSize) {
  //Generate JSON 
  size_t snstatus = snprintf_P(p_buffer, bufSize, PSTR(R"=="==({"channel":%.1i,"mode":"%.3s"}
)=="==")
    , channel
    , (channels[channel - 1] == MODE_ON) ? "on" : "off"
  );
  assert(snstatus > 0 && snstatus < bufSize);
  return snstatus;
}

//void configModeCallback(WiFiManager *myWiFiManager) {
//  
//}

/**
 * 
**/
void configureWebParams() {
  // mind static keyword - effective global.
  static WiFiManagerParameter webparams[] = {
    WiFiManagerParameter("login",      "HTTP Login",      settings.login, AUTHBASIC_LEN_USERNAME),
    WiFiManagerParameter("password",   "HTTP Password",   settings.password, AUTHBASIC_LEN_PASSWORD/*, "type='password'"*/),
    WiFiManagerParameter("ssid",       "AP mode SSID",    settings.ssid, LENGTH_SSID),
    WiFiManagerParameter("wpa_key",    "AP mode WPA key", settings.wpa_key, LENGTH_WPA_KEY),
    WiFiManagerParameter("webservice", "Webservice", bool2str(settings.flags.webservice), 5, "placeholder=\"webservice\" type=\"checkbox\""),
    WiFiManagerParameter("wifimanager_portal", "WiFiManager Portal in STA mode", bool2str(settings.flags.wifimanager_portal), 5, "placeholder=\"wifimanager_portal\" type=\"checkbox\""),
  };
#ifdef WIFIMANAGER_HAS_SETPARAMETERS
  // WiFiManager v2.0.9 is built around an array with POINTERS (WiFiManagerParameter*[])... Wouldn't work without changing a few things.
  wifiManager.setParameters(&webparams);
#else
  for (WiFiManagerParameter &x : webparams) {
    wifiManager.addParameter(&x);
  }
#endif
}

void setup()  {
  Serial.begin(115200);
  
  // TODO: align to 256-Byte programmable pages (FLASH_PAGE_SIZE)
  // e.g. map 1st block, if invalid map 2nd...16th block. If 16th block is to be invalidated, erase 4K page and start from 1st block.
  EEPROM.begin(FLASH_SECTOR_SIZE);
  
  // Load settings from flash
  if (settings.loadSettings(settings_offset)) {
    logger.info(F("{'RemoteRelay': '%s'}"), REMOTERELAY_VERSION);
  } else {
    logger.info(F("{'RemoteRelay': '%s', 'mode': 'failsafe'}"), REMOTERELAY_VERSION);
  }
  // nop - don't need to save defaults in error case because they can be restored anytime. Save write cycles.
  
  // These are setters without unwanted side-effects.
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setRemoveDuplicateAPs(true);

  // Be sure the relays are in the default state (NC, off)
  #pragma clang loop unroll(full)
  //#pragma GCC unroll 4
  for (int8_t i = sizeof(channels); i > 0; --i) {
    // pucgenie: (i, --i) would violate -Wsequence-point
    setChannel(i, channels[i - 1]);
  }

  // don't think about freeing these resources if not using them - we would need to implement a good reset mechanism...
  configureWebParams();
  wifiManager.setSaveConfigCallback([](){
    shouldSaveConfig = true;
  });
  
  //wifiManager.setAPCallback(configModeCallback);
  
  #ifdef DISABLE_NUVOTON_AT_REPLIES
  myWiFiState = AUTO_REQUESTED;
  #endif
}

void loop() {
  switch (myLoopState) {
    case AFTER_SETUP:
      #ifndef DISABLE_NUVOTON_AT_REPLIES
      // wait for serial commands
      // TODO: light sleep and ignore "AT"/react on "+" (parsing serial input)?
      // handled in any case after switch
      #endif
      // nop
    break;
    // pucgenie: fully implemented
    case SHUTDOWN_REQUESTED: {
      delay(3000);
      myLoopState = SHUTDOWN_HALT;
    }
    break;
    // pucgenie: fully implemented
    case RESTART_REQUESTED: {
      delay(3000);
      myLoopState = SHUTDOWN_RESTART;
    }
    break;
    // pucgenie: fully implemented
    case SHUTDOWN_HALT: {
      logger.info(F("{'action': 'powering down'}"));
      ESP.deepSleep(0);
    }
    break;
    // pucgenie: fully implemented
    case SHUTDOWN_RESTART: {
      logger.info(F("{'action': 'restarting'}"));
      ESP.restart();
    }
    break;
    // unused
    case ERASE_EEPROM: {
      // spi_flash_geometry.h, FLASH_SECTOR_SIZE 0x1000
      // TODO: implement it?
      
      myLoopState = AFTER_SETUP;
    }
    break;
    case RESTORE: {
      logger.info(F("{'action': 'destroying settings in EEPROM...'}"));
      myLoopState = EEPROM_DESTROY_CRC;
    }
    break;
    case RESET: {
      // nop - because CWMODE is sent BEFORE reset request -.-
      // TODO: maybe turn WiFi off again?
      myLoopState = AFTER_SETUP;
    }
    break;
    default: {
      logger.info(F("{'LoopState': 'invalid'}"));
      led_scream(0b10010010);
      myLoopState = SHUTDOWN_REQUESTED;
    }
    break;
    case EEPROM_DESTROY_CRC: {
      RemoteRelaySettings::eeprom_destroy_crc(settings_offset);
      // where to commit then?
      EEPROM.commit();
      myLoopState = RESTART_REQUESTED;
    }
    break;
    case SAVE_SETTINGS: {
      shouldSaveConfig = false;
      settings.saveSettings(settings_offset);
      myLoopState = AFTER_SETUP;
    }
    break;
  }

  switch (myWiFiState) {
    case AP_REQUESTED:
      wifiManager.disconnect();
      wifiManager.setCaptivePortalEnable(true);
      WiFi.mode(WIFI_AP);
      wifiManager.setEnableConfigPortal(true);
      wifiManager.setSaveConnect(false);
      wifiManager.startConfigPortal(settings.ssid, settings.wpa_key);
      myWiFiState = AP_MODE;
    break;
    case DO_AUTOCONNECT: {
      wifiManager.setSaveConnect(settings.flags.wifimanager_portal);
      wifiManager.setEnableConfigPortal(!settings.flags.wifimanager_portal);
      // FIXME: enable in AP mode
      //wifiManager.setCaptivePortalEnable(false);
      // Connect to Wifi or ask for SSID
      bool res = wifiManager.autoConnect(settings.ssid, settings.wpa_key);
      /*
      wifiManager.setConfigPortalTimeout(timeout);
      wifiManager.startConfigPortal(settings.ssid);
      wifiManager.startWebPortal();
      */
      if (res) {
        myWiFiState = STA_MODE;
        serial_response_next = F("WIFI CONNECTED\r\nWIFI GOT IP");
      } else {
        myWiFiState = MYWIFI_OFF;
        // dead
        myLoopState = SHUTDOWN_REQUESTED;
      }
    }
    break;
    case STA_REQUESTED: {
      //wifiManager.setCaptivePortalEnable(false);
      wifiManager.disconnect();
      WiFi.mode(WIFI_STA);
      myWiFiState = DO_AUTOCONNECT;
    }
    break;
    case AUTO_REQUESTED: {
      wifiManager.disconnect();
      WiFi.mode(WIFI_AP_STA);
      myWiFiState = DO_AUTOCONNECT;
    }
    break;
    case AP_MODE:
      // FIXME: what to do?
    break;
    case STA_MODE:
      if (WiFi.status() != WL_CONNECTED) {
        // TODO: connection lost
      }
    break;
    case MYWIFI_OFF: {
      
    }
    break;
  }

  switch (myWebState) {
    // TODO: don't assume WiFiManager portal is running!
    case WEB_REQUESTED:
      // Display local ip
      logger.info(F("{'IPAddress': '%s'}"), WiFi.localIP().toString().c_str());

      if (settings.flags.webservice) {
        setup_web_handlers(sizeof(channels));
        /* wifiManager handles server
        server.begin();
        */
      }
      
      if (settings.flags.wifimanager_portal || (myWiFiState == AP_MODE && (!settings.flags.webservice))) {
        wifiManager.startWebPortal();
      }
      logger.info(F("{'HTTPServer': 'started'}"));
      
      myWebState = WEB_FULL;
    break;
    case WEB_FULL:
    case WEB_CONFIG:
    case WEB_REST:
      /* now handled by wifiManager portal server service
      server.handleClient();
      */
      wifiManager.process();
      if (shouldSaveConfig) {
        myLoopState = SAVE_SETTINGS;
      }
    break;
    default: {
      
    }
    break;
  }

#ifndef DISABLE_NUVOTON_AT_REPLIES
  if (serial_response_next) {
    Serial.println(serial_response_next);
    serial_response_next = NULL;
  }
  if (myLoopState == AFTER_SETUP) {
    // don't accept serial commands if some action is queued. We have enought time to react at next loop iteration.
    static MyATCommand at_previous = INVALID_EXPECTED_AT, at_current;
    // pretend to be an AT device here
    if (Serial.available()) {
      switch (at_current = atreplies.handle_nuvoTon_comms(logger)) {
        case AT_RESTORE: {
          myLoopState = RESTORE;
          //serial_response_next = F("OK");
        }
        break;
        case AT_RST: {
          myLoopState = RESET;
            // pretend we reset (wait a bit then send the WiFi connected message)
            delay(10);
            Serial.println(F("WIFI CONNECTED\r\nWIFI GOT IP"));
        }
        break;
        case AT_CWMODE_1: {
          if (at_previous != AT_CWMODE_1) {
            if (myWiFiState == STA_MODE) {
              logger.logNow("{'myWiFiMode': 'unexpected state'}");
            }
            myWiFiState = STA_REQUESTED;
          }
        }
        break;
        case AT_CWMODE_2: {
          if (at_previous != AT_CWMODE_2) {
            myWiFiState = AP_REQUESTED;
          }
        }
        break;
        case AT_CWSTARTSMART: {
          
        }
        break;
        case AT_CWSMARTSTART_1: {
          
        }
        break;
        case AT_CIPMUX_1: {
          
        }
        break;
        case AT_CIPSERVER: {
          myWebState = WEB_REQUESTED;
        }
        break;
        case AT_CIPSTO: {
          
        }
        break;
        case INVALID_EXPECTED_AT: {
          
        }
        break;
        default: {
          
        }
        break;
      }
      at_previous = at_current;
    }
    if (serial_response_next) {
      Serial.println(serial_response_next);
      serial_response_next = NULL;
    }
  }
#endif
}
