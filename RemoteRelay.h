/*************************************************************************
 *
 * This file is part of the Remoterelay Arduino sketch.
 * Copyleft 2017 Nicolas Agius <nicolas.agius@lps-it.fr>
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

#ifndef REMOTERELAY_H
#define REMOTERELAY_H

/**
If enabled, also remove often used strings from RAM.
**/
#if 0
#define ULTRALOWMEMORY_FUNC snprintf_P
#define ULTRALOWMEMORY_STR PSTR
#else
#define ULTRALOWMEMORY_FUNC snprintf
#define ULTRALOWMEMORY_STR
#endif

/**
If enabled, remove not-that-often-used strings from RAM.
**/
#if 1
#define LOWMEMORY_FUNC snprintf_P
#define LOWMEMORY_STR PSTR
#else
#define LOWMEMORY_FUNC snprintf
#define LOWMEMORY_STR
#endif

#include "Logger.h"
#include "RemoteRelaySettings.h"

#define REMOTERELAY_VERSION "2.0"

#include <DNSServer.h>
#include <WiFiManager.h>         // See https://github.com/tzapu/WiFiManager for documentation
//#include <strings_en.h>

#include "RemoteRelay_creds.h"

#define RELAY_NUMBER_OF_CHANNELS 4
//deprecated: #define FOUR_WAY_MODE           // Enable channels 3 and 4 (comment out to disable)
#ifndef RELAY_NUMBER_OF_CHANNELS
  #ifdef FOUR_WAY_MODE
    #define RELAY_NUMBER_OF_CHANNELS 4
  #else
    #define RELAY_NUMBER_OF_CHANNELS 2
  #endif
#endif

//#define DISABLE NUVOTON_AT_REPLIES      // https://github.com/nagius/RemoteRelay/issues/4 (uncomment to disable feature)
#ifndef DISABLE_NUVOTON_AT_REPLIES
#include "ATReplies.h"
#endif

enum MyLoopState {
  // it means something like READY
  AFTER_SETUP,
  // delayed shutdown
  SHUTDOWN_REQUESTED,
  RESTART_REQUESTED,
  // shutdown now
  SHUTDOWN_HALT,
  SHUTDOWN_RESTART,
  // write all 1s to used EEPROM flash page. If it was bitwise EEPROM, would have just stored an invalid CRC value instead.
  ERASE_EEPROM,
  // AT+RESTORE received
  RESTORE,
  // AT+RST received (when switching AT+CWMODE. nuvoTon tries up to 3 times about every 28 seconds)
  RESET,
  EEPROM_DESTROY_CRC,
  SAVE_SETTINGS,
};

enum MyWiFiState {
  AP_REQUESTED,
  STA_REQUESTED,
  AP_MODE,
  STA_MODE,
  // fallback operation, autoConnect
  AUTO_REQUESTED,
  DO_AUTOCONNECT,
  MYWIFI_OFF,
};

enum MyWebState {
  // nuvoTon sends the same commands regardless of CWMODE (but CWMODE=1 waits for "WIFI GOT IP" to be received by nuvoTon)
  WEB_REQUESTED,
  WEB_FULL,
  WEB_CONFIG,
  WEB_REST,
  WEB_DISABLED,
};

enum MyPingState {
  PING_NONE,
  PING_BACKGROUND,
  PING_RECEIVED,
  PING_TIMEOUT,
};

// Used for string buffers
#define BUF_SIZE 384
// TODO: Create a manager for retrieving and returning buffers?
// Global char* to avoid multiple String concatenation which causes RAM fragmentation
extern char buffer[2][BUF_SIZE];

// Global variables
//extern ESP8266WebServer server;
extern Logger logger;
extern RemoteRelaySettings settings;
extern bool shouldSaveConfig;    // Flag for WifiManager custom parameters
extern MyLoopState myLoopState;
extern MyWiFiState myWiFiState;
extern MyWebState myWebState;
extern MyPingState myPingState;
extern WiFiManager wifiManager;

// See LC-Relay board datasheet for open/close values
enum RSTM32Mode {
  R_OPEN  = 0, // OFF
  R_CLOSE = 1, // ON
};

void setChannel(const uint8_t channel, const RSTM32Mode mode);
//void saveSettings(RemoteRelaySettings &p_settings, uint16_t &p_settings_offset);
void eeprom_destroy_crc(uint16_t &old_addr);
// Doesn't need to be visible yet.
//bool loadSettings(RemoteRelaySettings &p_settings, uint16_t &out_address);
//void setDefaultSettings(RemoteRelaySettings& p_settings);
/**
 * @returns count of chars written (without terminator)
**/
size_t getJSONState(const uint8_t channel, char * const p_buffer, const size_t bufSize);

#endif  // REMOTERELAY_H
