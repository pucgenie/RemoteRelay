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

#include "Logger.h"

#include <DNSServer.h>
#include <WiFiManager.h>         // See https://github.com/tzapu/WiFiManager for documentation
#include <strings_en.h>

// Default value
#define DEFAULT_LOGIN "sxabc"        // AuthBasic credentials
#define DEFAULT_PASSWORD "MtsssezPzg"
#define DEFAULT_STANDALONE_SSID "RemoteRelay"
#define DEFAULT_STANDALONE_WPA_KEY "1234x5678"

#define FOUR_WAY_MODE           // Enable channels 3 and 4 (comment out to disable)

//#define DISABLE NUVOTON_AT_REPLIES      // https://github.com/nagius/RemoteRelay/issues/4 (uncomment to disable feature)
#ifndef DISABLE_NUVOTON_AT_REPLIES
#include "ATReplies.h"
#endif

// Internal constant
#define AUTHBASIC_LEN_USERNAME 20        // Login or password 20 char max
#define AUTHBASIC_LEN_PASSWORD 20        // Login or password 20 char max
#define LENGTH_SSID 32
#define LENGTH_WPA_KEY 64
#define VERSION "2.0"

#define MODE_ON 1               // See LC-Relay board datasheet for open/close values
#define MODE_OFF 0

// 512 bytes mapped (EEPROM.begin).
struct ST_SETTINGS {
  public:
    union {
      struct {
        /**
         * Count the number of zeroes to rush along the linked list.
         * Remember: Setting a 1 to a 0 doesn't need to erase the sector (4kiB).
         * If it is full (all zeroes), it is not implemented to check those bits in following settings blocks.
         */
        int16_t wearlevel_mark    :4;
        /**
         * Output debug messages
        **/
        int16_t debug             :1;
        /**
         * Log output to serial port
        **/
        int16_t serial            :1;
        /**
         * If set, webservice will be brought up on nuvoTon serial command or on boot if compiled with DISABLE_NUVOTON_AT_REPLIES:
           AT+CIPMUX=1
           AT+CIPSERVER=1,8080
           AT+CIPSTO=360
         *
         * If disabled, µC may sleep between ping pong intervals.
         */
        int16_t webservice        :1;
        int16_t wifimanager_portal:1;

        int16_t erase_cycles      :8;
      };
      // each member needs to have the same type that the full bitfield has
      int16_t reg;
    } flags;
    char login[AUTHBASIC_LEN_USERNAME+1];
    char password[AUTHBASIC_LEN_PASSWORD+1];
    /**
     * The access point's SSID.
     */
    char ssid[LENGTH_SSID+1];
    /**
     * The access point's password.
     */
    char wpa_key[LENGTH_WPA_KEY+1];
};

enum MyLoopState {
  // first loop call
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

#define BUF_SIZE 384            // Used for string buffers
extern char buffer[];             // Global char* to avoid multiple String concatenation which causes RAM fragmentation

// Global variables
//extern ESP8266WebServer server;
extern Logger logger;
extern struct ST_SETTINGS settings;
extern bool shouldSaveConfig;    // Flag for WifiManager custom parameters
extern MyLoopState myLoopState;
extern MyWiFiState myWiFiState;
extern MyWebState myWebState;
extern WiFiManager * const wifiManager;

void setChannel(uint8_t channel, uint8_t mode);
//void saveSettings(struct ST_SETTINGS &p_settings, uint16_t &p_settings_offset);
void eeprom_destroy_crc(uint16_t &old_addr);
// Doesn't need to be visible yet.
//bool loadSettings(struct ST_SETTINGS &p_settings, uint16_t &out_address);
//void setDefaultSettings(struct ST_SETTINGS& p_settings);
void getJSONSettings(char *buffer, size_t bufSize);
void getJSONState(uint8_t channel, char *p_buffer, size_t bufSize);

#endif  // REMOTERELAY_H
