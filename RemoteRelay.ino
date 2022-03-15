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

#include <spi_flash.h>
#include <EEPROM.h>
#include <AsyncPing.h>

#include "RemoteRelay.h"
char buffer[BUF_SIZE];

#include "WebHelper.h"
#include "ledsignalling.h"

#include "syntacticsugar.h"

//ESP8266WebServer server(80);
// contained in wifiManager->server->

struct ST_SETTINGS settings;
Logger logger;
bool shouldSaveConfig = false;
MyLoopState myLoopState = AFTER_SETUP;
MyWiFiState myWiFiState = MYWIFI_OFF;
MyWebState myWebState = WEB_DISABLED;
/**
 * WeFiManagerParameters can't be removed so deleting the whole object is necessary.
**/
WiFiManager * const wifiManager = new WiFiManager();
static AsyncPing ping;
static const __FlashStringHelper *serial_response_next = NULL;
// TODO: should be configurable
static IPAddress isp_endpoint(8,8,8,8);

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

static uint16_t settings_offset;

/**
 * Flash memory helpers 
 ********************************************************************************/

// CRC8 simple calculation
// Based on https://github.com/PaulStoffregen/OneWire/blob/master/OneWire.cpp
uint8_t crc8(const uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;

  while (len--) {
    uint8_t inbyte = *addr++;
    #pragma clang loop unroll(full)
    #pragma GCC unroll 8
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

#define SETTINGS_FALSH_SIZE sizeof(struct ST_SETTINGS)+1
#define SETTINGS_FALSH_OVERADDR FLASH_SECTOR_SIZE - (SETTINGS_FALSH_SIZE)

/**
 * Reads settings from EEPROM flash into p_settings.
 * Returns the byte start location of the loaded settings block.
 * 
 * @param the_address Should be 0 to load the first valid settings block. Can be an exact address too.
 */
bool loadSettings(struct ST_SETTINGS &p_settings, uint16_t &the_address) {
  int x;
  bool ret;
  while (ret = (the_address < (SETTINGS_FALSH_OVERADDR))) {
    EEPROM.get(the_address, p_settings);
    // check if marked as deleted and how many bits are set 0
    x = p_settings.flags.wearlevel_mark;
    if (x < ((1<<GET_BIT_FIELD_WIDTH(ST_SETTINGS, flags.wearlevel_mark)) - 1)) {
      #pragma clang loop unroll(full)
      #pragma GCC unroll 8
      for (int nb = GET_BIT_FIELD_WIDTH(ST_SETTINGS, flags.wearlevel_mark); nb --> 0; ) {
        // some way to spare one instruction?^^
        if (x & (1<<nb) == 0) {
          the_address += sizeof(struct ST_SETTINGS) + 1;
        }
      }
    } else if (crc8((uint8_t*) &p_settings, sizeof(struct ST_SETTINGS)) == uint8_t(EEPROM.read(the_address + sizeof(struct ST_SETTINGS)))) {
      // index of valid settings found
      EEPROM.get(the_address, p_settings);
  break;
    } else {
      the_address += sizeof(struct ST_SETTINGS) + 1;
    }
  }
  if (!ret) {
    logger.info(F("{'settings': 'loading default'}"));
    //setDefaultSettings(p_settings);
    strncpy_P(p_settings.login, PSTR(DEFAULT_LOGIN), AUTHBASIC_LEN_USERNAME+1);
    strncpy_P(p_settings.password, PSTR(DEFAULT_PASSWORD), AUTHBASIC_LEN_PASSWORD+1);
    strncpy_P(p_settings.ssid, PSTR(DEFAULT_STANDALONE_SSID), LENGTH_SSID+1);
    strncpy_P(p_settings.wpa_key, PSTR(DEFAULT_STANDALONE_WPA_KEY), LENGTH_WPA_KEY+1);
    p_settings.flags.wearlevel_mark = ~0;
    p_settings.flags.erase_cycles = 0;
    p_settings.flags.debug = false;
    p_settings.flags.serial = false;
    p_settings.flags.wifimanager_portal = true;
    p_settings.flags.webservice = true;
    
    led_scream(0b10101010);
    
    the_address = 0;
  } else {
    // serial is disabled by default, so spare us another if after setting
    logger.info(F("{'settings': 'loaded from flash'}"));
  }
  // could have changed
  logger.setSerial(p_settings.flags.serial);

  // Display loaded setting on debug
  if (settings.flags.debug) {
    getJSONSettings(buffer, BUF_SIZE);
    logger.logNow(buffer);
  }
  return ret;
}

/**
 * write a zero anywhere in CRC to force loading default settings at boot
 */
void eeprom_destroy_crc(uint16_t &old_addr) {
  struct ST_SETTINGS tmp_settings;
  if (!loadSettings(tmp_settings, old_addr)) {
    // it already is incorrect - nop.
return;
  }
  tmp_settings.flags.wearlevel_mark <<= 1;
  uint8_t crc = EEPROM.read(old_addr + sizeof(struct ST_SETTINGS));
  if (crc == crc8((uint8_t*) &tmp_settings, sizeof(struct ST_SETTINGS))) {
    if (crc == 0) {
      // what a coincidence
      bool found = false;
      char *ptr = tmp_settings.login;
      if (offsetof(ST_SETTINGS, password) < offsetof(ST_SETTINGS, login)) {
        // FIXME: need a define for char[] capacities in ST_SETTINGS...
        while (true) {
          led_scream(0b11001100);
        }
        // # error ERROR ST_SETTINGS members changed unpredictably
      }
      static const uint16_t OVERWRITABLE_BYTES[] = {
        offsetof(ST_SETTINGS, password) - offsetof(ST_SETTINGS, login),
        offsetof(ST_SETTINGS, ssid) - offsetof(ST_SETTINGS, password),
        offsetof(ST_SETTINGS, wpa_key) - offsetof(ST_SETTINGS, ssid),
        sizeof(((ST_SETTINGS *)0)->wpa_key)
      };
      bool string_terminator_found;
      byte owf_i = sizeof(OVERWRITABLE_BYTES);
      while ((!found) && (owf_i --> 0)) {
        string_terminator_found = false;
        // pucgenie: I wonder whether or not the compiler sees
        #pragma clang loop unroll(full)
        //#pragma GCC unroll 255
        for (int i = OVERWRITABLE_BYTES[owf_i]; i --> 0; ++ptr) {
          if ((*ptr) == 0) {
            string_terminator_found = true;
          } else if (string_terminator_found) {
            (*ptr) = 0;
            // We are certain that changing a single bit changes the resulting CRC too.
            found = true;
        break;
            }
        }
      }
      // don't touch ST_SETTINGS.erase_cycles
      if (!found) {
         // FIXME: erase
         while (true) {
          led_scream(0b11101110);
        }
      }
    } else {
      // Arduino.h byte is unsigned
      byte i = 0b10000000;
      while (i > 0 && (crc & i) == 0) {
        i >>= 1;
      }
      //assert i != 0;
      crc ^= i;
      // it can't possibly happen that it doesn't find a bit because 0 case was handled before.
      EEPROM.write(old_addr + sizeof(struct ST_SETTINGS), crc);
    }
    EEPROM.put(old_addr, tmp_settings);
    // don't commit
  }
}

void saveSettings(struct ST_SETTINGS &p_settings, uint16_t &p_settings_offset) {
  #ifdef EEPROM_SPI_NOR_REPROGRAM
  {
    uint16_t old_addr = p_settings_offset;
    eeprom_destroy_crc(old_addr);
  }
  #endif
  uint8_t theCRC = crc8((uint8_t*) &p_settings, sizeof(struct ST_SETTINGS));
  p_settings_offset += SETTINGS_FALSH_SIZE;
  if (p_settings_offset >= (SETTINGS_FALSH_OVERADDR - SETTINGS_FALSH_SIZE)) {
    p_settings.flags.erase_cycles += 1;
    p_settings_offset = 0;
    //TODO: erase sector explicitly or keep using EEPROM class' auto-detection?
  }
  EEPROM.put(p_settings_offset, p_settings);
  EEPROM.put(p_settings_offset + sizeof(struct ST_SETTINGS), theCRC);
  EEPROM.commit();
}

#undef SETTINGS_FALSH_OVERADDR
#undef SETTINGS_FALSH_SIZE

//void setDefaultSettings(struct ST_SETTINGS& p_settings)

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
  
  static_assert(sizeof(channels) <= 9, "print functions are restricted to one-digit channel count");
  // Save status 
  channels[channel - 1] = mode;
  
  logger.info(F("{'channel': %.1i, 'state': '%.3s'}"), channel, (mode == MODE_ON) ? "on" : "off");
  logger.debug(F("{'payload': '%02X%02X%02X%02X'}"), payload[0], payload[1], payload[2], payload[3]);
  
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
  int snstatus = snprintf_P(p_buffer, bufSize, PSTR(R"=="==({"login":"%s","password":"<hidden>","debug":%.5s,"serial":%.5s,"webservice":%.5s,"wifimanager_portal":%.5s}
)=="==")
    , settings.login
    , bool2str(settings.flags.debug)
    , bool2str(settings.flags.serial)
    , bool2str(settings.flags.webservice)
    , bool2str(settings.flags.wifimanager_portal)
  );
  assert(snstatus < 0 || ((size_t /* interpret unsigned */) snstatus) > bufSize);
}

void getJSONState(uint8_t channel, char p_buffer[], size_t bufSize) {
  //Generate JSON 
  int snstatus = snprintf_P(p_buffer, bufSize, PSTR(R"=="==({"channel":%.1i,"mode":"%.3s"}
)=="==")
    , channel
    , (channels[channel - 1] == MODE_ON) ? "on" : "off"
  );
  assert(snstatus < 0 || snstatus > bufSize);
}

void configModeCallback(WiFiManager *myWiFiManager) {
  
}

/**
 * 
**/
void configureWebParams() {
  static WiFiManagerParameter webparams[] = {
    WiFiManagerParameter("login",      "HTTP Login",    settings.login, AUTHBASIC_LEN_USERNAME),
    WiFiManagerParameter("password",   "HTTP Password", settings.password, AUTHBASIC_LEN_PASSWORD/*, "type='password'"*/),
    WiFiManagerParameter("ssid",       "AP mode SSID",  settings.ssid, LENGTH_SSID),
    WiFiManagerParameter("wpa_key",    "AP mode WPA key", settings.wpa_key, LENGTH_WPA_KEY),
    WiFiManagerParameter("webservice", "Webservice", bool2str(settings.flags.webservice), 5, "placeholder=\"webservice\" type=\"checkbox\""),
    WiFiManagerParameter("wifimanager_portal", "WiFiManager Portal in STA mode", bool2str(settings.flags.wifimanager_portal), 5, "placeholder=\"wifimanager_portal\" type=\"checkbox\""),
  };
  for (WiFiManagerParameter &x : webparams) {
    wifiManager->addParameter(&x);
  }
}

void setup()  {
  Serial.begin(115200);
  
  // TODO: align to 256 Byte programmable blocks
  // e.g. map 1st block, if invalid map 2nd...16th block. If 16th block is to be invalidated, erase 4K page and start from 1st block.
  EEPROM.begin(512);
  
  // Load settigns from flash
  if (loadSettings(settings, settings_offset)) {
    logger.info(F("{'RemoteRelay': '%s'}"), VERSION);
  } else {
    logger.info(F("{'RemoteRelay': '%s', 'mode': 'failsafe'}"), VERSION);
  }
  // nop - don't need to save defaults in error case because they can be restored anytime. Save write cycles.
  
  // These are setters without unwanted side-effects.
  wifiManager->setConfigPortalBlocking(false);
  wifiManager->setRemoveDuplicateAPs(true);

  // Be sure the relay are in the default state (NC, off)
  #pragma clang loop unroll(full)
  #pragma GCC unroll 4
  for (uint8_t i = sizeof(channels); i > 0; --i) {
    // pucgenie: (i, --i) would violate -Wsequence-point
    setChannel(i, channels[i - 1]);
  }

  // don't think about freeing these resources if not using them - we would need to implement a good reset mechanism...
  // Configure custom parameters, store in HEAP (new). Maybe just make them static...
  configureWebParams();
  wifiManager->setSaveConfigCallback([](){
    shouldSaveConfig = true;
  });
  
  wifiManager->setAPCallback(configModeCallback);
  
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
      eeprom_destroy_crc(settings_offset);
      // where to commit then?
      EEPROM.commit();
      myLoopState = RESTART_REQUESTED;
    }
    break;
    case SAVE_SETTINGS: {
      shouldSaveConfig = false;
      saveSettings(settings, settings_offset);
      myLoopState = AFTER_SETUP;
    }
    break;
  }

  switch (myWiFiState) {
    case AP_REQUESTED:
      wifiManager->disconnect();
      wifiManager->setCaptivePortalEnable(true);
      WiFi.mode(WIFI_AP);
      wifiManager->setEnableConfigPortal(true);
      wifiManager->setSaveConnect(false);
      wifiManager->startConfigPortal(settings.ssid, settings.wpa_key);
      myWiFiState = AP_MODE;
    break;
    case DO_AUTOCONNECT: {
      if (settings.flags.wifimanager_portal) {
        wifiManager->setSaveConnect(false);
        wifiManager->setEnableConfigPortal(true);
      } else {
        wifiManager->setEnableConfigPortal(false);
      }
      // FIXME: enable in AP mode
      //wifiManager->setCaptivePortalEnable(false);
      // Connect to Wifi or ask for SSID
      bool res = wifiManager->autoConnect(settings.ssid, settings.wpa_key);
      /*
      wifiManager->setConfigPortalTimeout(timeout);
      wifiManager->startConfigPortal(settings.ssid);
      wifiManager->startWebPortal();
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
      //wifiManager->setCaptivePortalEnable(false);
      wifiManager->disconnect();
      WiFi.mode(WIFI_STA);
      myWiFiState = DO_AUTOCONNECT;
    }
    break;
    case AUTO_REQUESTED: {
      wifiManager->disconnect();
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
        wifiManager->startWebPortal();
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
      wifiManager->process();
      if (shouldSaveConfig) {
        myLoopState = SAVE_SETTINGS;
      }
    break;
    default: {
      
    }
    break;
  }

#ifndef DISABLE_NUVOTON_AT_REPLIES
  if (myLoopState == AFTER_SETUP) {
    // don't accept serial commands if some action is queued. We have enought time to react at next loop iteration.
    static MyATCommand at_previous = INVALID_EXPECTED_AT, at_current;
    // pretend to be an AT device here
    if (Serial.available()) {
      switch (at_current = atreplies.handle_nuvoTon_comms(logger)) {
        AT_RESTORE: {
          myLoopState = RESTORE;
        }
        break;
        AT_RST: {
          myLoopState = RESET;
        }
        break;
        AT_CWMODE_1: {
          if (at_previous != AT_CWMODE_1) {
            if (myWiFiState == STA_MODE) {
              logger.logNow("{'myWiFiMode': 'unexpected state'}");
            }
            myWiFiState = STA_REQUESTED;
          }
        }
        break;
        AT_CWMODE_2: {
          if (at_previous != AT_CWMODE_2) {
            myWiFiState = AP_REQUESTED;
          }
        }
        break;
        AT_CWSTARTSMART: {
          
        }
        break;
        AT_CWSMARTSTART_1: {
          
        }
        break;
        AT_CIPMUX_1: {
          
        }
        break;
        AT_CIPSERVER: {
          myWebState = WEB_REQUESTED;
        }
        break;
        AT_CIPSTO: {
          
        }
        break;
        default: {
          
        }
        break;
        INVALID_EXPECTED_AT: {
          
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
