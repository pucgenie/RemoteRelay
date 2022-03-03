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

#define FOUR_WAY_MODE           // Enable channels 3 and 4 (comment out to disable)

//#define DISABLE NUVOTON_AT_REPLIES      // https://github.com/nagius/RemoteRelay/issues/4 (uncomment to disable feature)
#ifndef DISABLE_NUVOTON_AT_REPLIES
#include "ATReplies.h"
#endif

// Internal constant
#define AUTHBASIC_LEN_USERNAME 20+1        // Login or password 20 char max
#define AUTHBASIC_LEN_PASSWORD 20+1        // Login or password 20 char max
#define VERSION "2.0"

#define MODE_ON 1               // See LC-Relay board datasheet for open/close values
#define MODE_OFF 0

#define ST_SETTINGS_WATERMARK_BITS 4

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
        byte wearlevel_mark    :ST_SETTINGS_WATERMARK_BITS;
        byte debug             :1;
        byte serial            :1;
        /**
         * If set, webservice will be brought up on nuvoTon serial command or on boot if compiled with DISABLE_NUVOTON_AT_REPLIES:
           AT+CIPMUX=1
           AT+CIPSERVER=1,8080
           AT+CIPSTO=360
         *
         * If not set, µC may sleep between ping pong intervals.
         * If switch 1 is pressed/hold during boot, 
         */
        byte webservice        :1;
        byte wifimanager_portal:1;
      };
      // each member needs to have the same type that the full bitfield has
      byte reg;
    } flags;
    char login[AUTHBASIC_LEN_USERNAME];
    char password[AUTHBASIC_LEN_PASSWORD];
    /**
     * The access point's SSID.
     */
    char ssid[32+1];
    /**
     * The access point's password.
     */
    char wpa_key[64+1];
    long erase_cycles;
};

enum MyLoopState {
  // first loop call
  AFTER_SETUP,
  // config mode, full webservices for testing
  AP_MODE,
  // no webservices
  STA_LITE,
  // normal operation
  STA_WEB,
  // fallback operation, autoConnect
  AUTO_REQUESTED,
  // delayed shutdown
  SHUTDOWN_REQUESTED,
  // shutdown now
  SHUTDOWN_HALT,
  // write all 1s to used EEPROM flash page. If it was bitwise EEPROM, would have just stored an invalid CRC value instead.
  ERASE_EEPROM,
  // AT+RESTORE received
  RESTORE,
  // AT+RST received (when switching AT+CWMODE. nuvoTon tries up to 3 times about every 28 seconds)
  RESET,
  // wants AP_MODE
  AP_REQUESTED,
  // wants STA_{LITE|WEB}
  STA_REQUESTED,
  // nuvoTon sends the same commands regardless of CWMODE (but CWMODE=1 waits for "WIFI GOT IP" to be received by nuvoTon)
  WEB_REQUESTED,
};

#define BUF_SIZE 384            // Used for string buffers
extern char buffer[];             // Global char* to avoid multiple String concatenation which causes RAM fragmentation

// Global variables
//extern ESP8266WebServer server;
extern Logger logger;
extern struct ST_SETTINGS settings;
extern bool shouldSaveConfig;    // Flag for WifiManager custom parameters
extern MyLoopState myLoopState;
extern WiFiManager wifiManager;

void setChannel(uint8_t channel, uint8_t mode);
void saveSettings(struct ST_SETTINGS &p_settings);
void eeprom_destroy_crc();
// Doesn't need to be visible yet.
//long loadSettings(struct ST_SETTINGS &p_settings);
//void setDefaultSettings(struct ST_SETTINGS& p_settings);
void getJSONSettings(char buffer[], size_t bufSize);
void getJSONState(uint8_t channel, char p_buffer[], size_t bufSize);

#endif  // REMOTERELAY_H
