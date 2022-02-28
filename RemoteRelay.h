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

// Default value
#define DEFAULT_LOGIN "sxabc"        // AuthBasic credentials
#define DEFAULT_PASSWORD "MtsssezPzg"

#define FOUR_WAY_MODE           // Enable channels 3 and 4 (comment out to disable)

#define NUVOTON_AT_REPLIES      // https://github.com/nagius/RemoteRelay/issues/4 (comment out to disable)
#ifdef NUVOTON_AT_REPLIES
#include "ATReplies.h"
#endif

// Internal constant
#define AUTHBASIC_LEN_USERNAME 20+1        // Login or password 20 char max
#define AUTHBASIC_LEN_PASSWORD 20+1        // Login or password 20 char max
#define VERSION "1.3"

#define MODE_ON 1               // See LC-Relay board datasheet for open/close values
#define MODE_OFF 0

// 512 bytes available.
struct ST_SETTINGS {
  bool debug;
  bool serial;
  /**
   * If set, webservice will be brought up on nuvoTon serial command or on boot if compiled without NUVOTON_AT_REPLIES:
     AT+CIPMUX=1
     AT+CIPSERVER=1,8080
     AT+CIPSTO=360
   *
   * If not set, µC may sleep between ping pong intervals.
   * If switch 1 is pressed/hold during boot, 
   */
  bool webservice;
  char login[AUTHBASIC_LEN_USERNAME];
  char password[AUTHBASIC_LEN_PASSWORD];
  char ssid[32+1];
};

struct ST_SETTINGS_FLAGS {
  bool debug;
  bool serial;
  bool login;
  bool password;
};

#include <ESP8266WebServer.h>

#define BUF_SIZE 256            // Used for string buffers
extern char buffer[];             // Global char* to avoid multiple String concatenation which causes RAM fragmentation

// Global variables
extern ESP8266WebServer server;
extern Logger logger;
extern struct ST_SETTINGS settings;
extern bool shouldSaveConfig;    // Flag for WifiManager custom parameters

void setChannel(uint8_t channel, uint8_t mode);
void resetWiFiManager();
void saveSettings(struct ST_SETTINGS &p_settings);
// Doesn't need to be visible yet.
//int loadSettings(struct ST_SETTINGS &p_settings);
void setDefaultSettings(struct ST_SETTINGS& p_settings);
void getJSONSettings(char buffer[], size_t bufSize);
char* getJSONState(uint8_t channel);

#endif  // REMOTERELAY_H
