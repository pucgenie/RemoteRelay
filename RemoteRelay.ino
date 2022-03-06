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
#include <Pinger.h>
extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
}

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
MyWiFiState myWiFiState = MYWIFI_OFF;
MyWebState myWebState = WEB_DISABLED;
WiFiManager wifiManager;
static Pinger pinger;
static const ____FlashStringHelper *serial_response_next = null;

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
    strncpy_P(p_settings.ssid, PSTR("RemoteRelay"), LENGTH_SSID+1);
    strncpy_P(p_settings.wpa_key, PSTR("1234x5678"), LENGTH_WPA_KEY+1);
    p_settings.flags.wearlevel_mark = ~0;
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
        offsetof(ST_SETTINGS, erase_cycles) - offsetof(ST_SETTINGS, wpa_key),
      };
      bool string_terminator_found;
      byte owf_i = sizeof(OVERWRITABLE_BYTES);
      while ((!found) && (owf_i --> 0)) {
        string_terminator_found = false;
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
    p_settings.erase_cycles += 1;
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

void configModeCallback(WiFiManager *myWiFiManager) {
  
}

void setup()  {
  Serial.begin(115200);
  EEPROM.begin(512);
  
  // Load settigns from flash
  if (loadSettings(settings, settings_offset)) {
    logger.info(F("{'RemoteRelay': '%s'}"), VERSION);
  } else {
    logger.info(F("{'RemoteRelay': '%s', 'mode': 'failsafe'}"), VERSION);
  }
  // nop - don't need to save defaults in error case because they can be restored anytime. Save write cycles.
  
  // These are setters without unwanted side-effects.
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setRemoveDuplicateAPs(true);

  // Be sure the relay are in the default state (off)
  for (uint8_t i = sizeof(channels); i > 0; ) {
    setChannel(i, channels[--i]);
  }

  // don't think about freeing these resources if not using them - we would need to implement a good reset mechanism...
  // Configure custom parameters
  WiFiManagerParameter http_login("htlogin", "HTTP Login", settings.login, AUTHBASIC_LEN_USERNAME);
  WiFiManagerParameter http_password("htpassword", "HTTP Password", settings.password, AUTHBASIC_LEN_PASSWORD/*, "type='password'"*/);
  WiFiManagerParameter http_ssid("ht2ssid", "AP mode SSID", settings.ssid, LENGTH_SSID);
  WiFiManagerParameter http_wpa_key("ht2wpa_key", "AP mode WPA key", settings.wpa_key, LENGTH_WPA_KEY);
  wifiManager.setSaveConfigCallback([](){
    shouldSaveConfig = true;
  });
  wifiManager.addParameter(&http_login);
  wifiManager.addParameter(&http_password);
  wifiManager.addParameter(&http_ssid);
  wifiManager.addParameter(&http_wpa_key);
  
  wifiManager.setAPCallback(configModeCallback);

  pinger.OnReceive([](const PingerResponse& response)
  {
    if (response.ReceivedResponse) {
      Serial.printf(
        "Reply from %s: bytes=%d time=%lums TTL=%d\n",
        response.DestIPAddress.toString().c_str(),
        response.EchoMessageSize - sizeof(struct icmp_echo_hdr),
        response.ResponseTime,
        response.TimeToLive);
    } else {
      Serial.printf("Request timed out.\n");
    }

    // Return true to continue the ping sequence.
    // If current event returns false, the ping sequence is interrupted.
    return true;
  });

  pinger.OnEnd([](const PingerResponse& response)
  {
    // Evaluate lost packet percentage
    float loss = 100;
    if(response.TotalReceivedResponses > 0)
    {
      loss = (response.TotalSentRequests - response.TotalReceivedResponses) * 100 / response.TotalSentRequests;
    }
    
    // Print packet trip data
    Serial.printf(
      "Ping statistics for %s:\n",
      response.DestIPAddress.toString().c_str());
    Serial.printf(
      "    Packets: Sent = %lu, Received = %lu, Lost = %lu (%.2f%% loss),\n",
      response.TotalSentRequests,
      response.TotalReceivedResponses,
      response.TotalSentRequests - response.TotalReceivedResponses,
      loss);

    // Print time information
    if(response.TotalReceivedResponses > 0)
    {
      Serial.printf("Approximate round trip times in milli-seconds:\n");
      Serial.printf(
        "    Minimum = %lums, Maximum = %lums, Average = %.2fms\n",
        response.MinResponseTime,
        response.MaxResponseTime,
        response.AvgResponseTime);
    }
    
    // Print host data
    Serial.printf("Destination host data:\n");
    Serial.printf(
      "    IP address: %s\n",
      response.DestIPAddress.toString().c_str());
    if(response.DestMacAddress != nullptr)
    {
      Serial.printf(
        "    MAC address: " MACSTR "\n",
        MAC2STR(response.DestMacAddress->addr));
    }
    if(response.DestHostname != "")
    {
      Serial.printf(
        "    DNS name: %s\n",
        response.DestHostname.c_str());
    }

    return true;
  });

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
      wifiManager.disconnect();
      wifiManager.setCaptivePortalEnable(true);
      WiFi.mode(WIFI_AP);
      wifiManager.setEnableConfigPortal(true);
      wifiManager.setSaveConnect(false);
      wifiManager.startConfigPortal(settings.ssid, settings.wpa_key);
      myWiFiState = AP_MODE;
    break;
    case STA_REQUESTED: {
      wifiManager.setCaptivePortalEnable(false);
      wifiManager.disconnect();
      if (settings.flags.wifimanager_portal) {
        wifiManager.setSaveConnect(false);
        wifiManager.setEnableConfigPortal(true);
      } else {
        wifiManager.setEnableConfigPortal(false);
      }
      WiFi.mode(WIFI_STA);
      // Connect to Wifi or ask for SSID
      bool res = wifiManager.autoConnect(settings.ssid, settings.wpa_key);
      /*
      wifiManager.setConfigPortalTimeout(timeout);
      wifiManager.startConfigPortal(settings.ssid);
      wifiManager.startWebPortal();
      */
      if (res) {
        myWiFiState = STA_MODE;
        serial_response_next = F("WIFI CONNECTED");
      } else {
        myWiFiState = MYWIFI_OFF;
        // dead
        myLoopState = SHUTDOWN_REQUESTED;
      }
    }
    break;
    // TODO: missing something?
    case AUTO_REQUESTED:
      // STA uses autoConnect at the moment.
      myWiFiState = STA_REQUESTED;
    break;
    case AP_MODE:
      // FIXME: what to do?
    break;
    case STA_MODE:
      if (WiFi.status() != WL_CONNECTED) {
        // TODO: connection lost
      }
      if (pinger.Ping("technologytourist.com") == false) {
        
      }
      if (pinger.Ping(IPAddress(8,8,8,8)) == false) {
        
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
        serial_response_next = F("WIFI GOT IP");
      }
      logger.info(F("{'HTTPServer': 'started'}"));
      
      myWebState = WEB_FULL;
    break;
    case WEB_FULL: // fall-through
    case WEB_CONFIG: // fall-through
    case WEB_REST: // fall-through
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
  }
#endif
}
