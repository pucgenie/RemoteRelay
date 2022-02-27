/*************************************************************************
 *
 * This file is part of the Remoterelay Arduino sketch.
 * Copyleft 2018 Nicolas Agius <nicolas.agius@lps-it.fr>
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

#include <ESP8266WiFi.h>         
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         // See https://github.com/tzapu/WiFiManager for documentation
#include <EEPROM.h>
#include "Logger.h"

// Default value
#define DEFAULT_LOGIN "sxabc"        // AuthBasic credentials
#define DEFAULT_PASSWORD "MtsssezPzg"

#define FOUR_WAY_MODE           // Enable channels 3 and 4 (comment out to disable)

#define NUVOTON_AT_REPLIES      // https://github.com/nagius/RemoteRelay/issues/4 (comment out to disable)

// Internal constant
#define AUTHBASIC_LEN 20+1        // Login or password 20 char max
#define BUF_SIZE 256            // Used for string buffers
#define VERSION "1.3"

#define MODE_ON 1               // See LC-Relay board datasheet for open/close values
#define MODE_OFF 0

#define bool2str(x) x ? "true" : "false"
#define charnonempty(x) x[0] != 0

// 512 bytes available.
struct ST_SETTINGS {
  bool debug;
  bool serial;
  bool web_on_boot;
  char login[AUTHBASIC_LEN];
  char password[AUTHBASIC_LEN];
  char ssid[32+1];
  char passphrase[64+1];
};

static const WebParam const String[] = {
    "debug"
  , "login"
  , "password"
  , "serial"
  , "web_on_boot"
}

struct ST_SETTINGS_FLAGS {
  bool debug;
  bool serial;
  bool login;
  bool password;
};

// Global variables
static ESP8266WebServer server(80);
static Logger const logger = Logger();
static ST_SETTINGS settings;
static bool shouldSaveConfig = false;    // Flag for WifiManager custom parameters
static char const buffer[BUF_SIZE];            // Global char* to avoir multiple String concatenation which causes RAM fragmentation


static uint8_t const channels[] = {
    MODE_OFF
  , MODE_OFF
#ifdef FOUR_WAY_MODE
  , MODE_OFF
  , MODE_OFF
#endif
                      };

// pucgenie: We need the memory in any case because there is no path tree parser...
static const char const channelPath[] = {
    "/channel/1"
  , "/channel/2"
#ifdef FOUR_WAY_MODE
  , "/channel/3"
  , "/channel/4"
#endif
                      };

//assert sizeof(channels) <= 9: "print functions are restricted to one-digit channel count"

/**
 * HTTP route handlers
 *******************************************************************************
 */

/**
 * GET /
 */
void handleGETRoot() 
{
  // I always loved this HTTP code
  server.send(418, F("text/plain"), F("\
            _           \r\n\
         _,(_)._            \r\n\
    ___,(_______).          \r\n\
  ,'__.           \\    /\\_  \r\n\
 /,' /             \\  /  /  \r\n\
| | |              |,'  /   \r\n\
 \\`.|                  /    \r\n\
  `. :           :    /     \r\n\
    `.            :.,'      \r\n\
      `-.________,-'        \r\n\
  \r\n"));
}

/**
 * GET /debug
 */
void handleGETDebug() {
  if (!isAuthBasicOK()) {
return;
  }
 
  server.send(200, F("text/plain"), logger.getLog());
}

/**
 * GET /settings
 */
void handleGETSettings() {
  if (!isAuthBasicOK()) {
return;
  }
 
  server.send(200, F("application/json"), getJSONSettings());
}

static const WebParam const String[] = {
    "debug"
  , "login"
  , "password"
  , "serial"
}

uint8_t binarysearchString(String[] table, String value) {
  //assert table != NULL && value != NULL
  uint8_t lowerBound = 0, upperBound = sizeof(table);
  uint8_t pivot = upperBound / 2;
  while (lowerBound != upperBound; ) {
    String elem = table[pivot];
    // TODO: pucgenie: what data type does compareTo return??
    int diff = value.compareTo(elem);
    if (diff == 0) {
return pivot;
    }
    if (diff > 0) {
      lowerBound = pivot + 1;
    } else // assert if (diff < 0) {
      upperBound = pivot - 1;
    }
    pivot = lowerBound + (upperBound - lowerBound) / 2;
  }
  return -pivot - 1;
}

/**
 * POST /settings
 * Args :
 *   - debug = <bool>
 *   - login = <str>
 *   - password = <str>
 */
void handlePOSTSettings() {
  if (!isAuthBasicOK()) {
return;
  }
  // Check if args have been supplied
  if (server.args() == 0) {
    server.send(400, F("test/plain"), F("Invalid parameters\r\n"));
return;
  }
  ST_SETTINGS_FLAGS isNew = { false, false, false, false };

  // Parse args   
  for (uint8_t i = server.args(); i --> 0; ) {
    String param = server.argName(i);
    switch (binarysearchString(WebParam, param)) {
      default:
        server.send(400, F("text/plain"), "Unknown parameter: " + param + "\r\n");
return;
      case 0: // debug
        settings.debug = server.arg(i).equalsIgnoreCase("true");
        isNew.debug = true;
    break;
      case 1: // login
        server.arg(i).toCharArray(settings.login, AUTHBASIC_LEN);
        isNew.login = true;
    break;
      case 2: // password
        server.arg(i).toCharArray(settings.password, AUTHBASIC_LEN);
        isNew.password = true;
    break;
      case 3: // serial
        settings.serial = server.arg(i).equalsIgnoreCase("true");
        isNew.serial = true;
    break;
    }
  }

  // Save changes
  if (isNew.debug) {
    logger.setDebug(settings.debug);
    // space doesn't break anything
    logger.info("Updated debug to %.5s.", bool2str(settings.debug));
  }

  if (isNew.serial) {
    logger.setSerial(settings.serial);
    logger.info("Updated serial to %.5s.", bool2str(settings.serial));
  }

  if (isNew.login) {
    logger.info("Updated login to \"%s\".", settings.login);
  }

  if (isNew.password) {
    logger.info("Updated password.");
  }

  saveSettings(&settings);

  // Reply with current settings
  server.send(201, F("application/json"), getJSONSettings());
}

/**
 * POST /reset
 */
void handlePOSTReset() {
  if (!isAuthBasicOK()) {
return;
  }
  WiFiManager wifiManager;
  
  logger.info("Reset settings to default");
  
  //reset saved settings
  wifiManager.resetSettings();
  setDefaultSettings();
  saveSettings();
  
  // Send response now
  server.send(200, F("text/plain"), F("Reset OK"));
  
  delay(3000);
  logger.info("Restarting...");
  
  ESP.restart();
}

/**
 * PUT /channel/:id
 * Args :
 *   - mode = "<on|off>"
 */
void handlePUTChannel(uint8_t channel) {
  // TODO: pucgenie: Can't server handle this check itself?
  if (!isAuthBasicOK()) {
return;
  }
  
  // Check if args have been supplied
  // Check if requested arg has been suplied
  if (server.args() != 1 || server.argName(0) != "mode") {
    server.send(400, F("test/plain"), F("Invalid parameter\r\n"));
return;
  }

  uint8_t requestedMode = MODE_OFF; // Default in case of error
  String value = server.arg(0);
  if (value.equalsIgnoreCase("on")) {
    requestedMode = MODE_ON;
  } else if (value.equalsIgnoreCase("off")) {
    requestedMode = MODE_OFF;
  } else {
    server.send(400, F("text/plain"), "Invalid value: " + value + "\r\n");
return;
  } 

  // Give some time to the watchdog
  ESP.wdtFeed();
  yield();

  setChannel(channel, requestedMode);
  server.send(200, F("application/json"), getJSONState(channel));
}

/**
 * GET /channel/:id
 */
void handleGETChannel(uint8_t channel)
{
  if (!isAuthBasicOK()) {
return;
  }

  server.send(200, F("application/json"), getJSONState(channel));
}

/**
 * WEB helpers 
 ********************************************************************************/

bool isAuthBasicOK() {
  // Disable auth if not credential provided
  if (charnonempty(settings.login) && charnonempty(settings.password)
      && !server.authenticate(settings.login, settings.password)) {
    server.requestAuthentication();
return false;
  }
  return true;
}

char* getJSONSettings() {
  //Generate JSON 
  snprintf(buffer, BUF_SIZE, R"=="==({"login":"%s","password":"<hidden>","debug":%.5s,"serial":%.5s}
)=="=="
    , settings.login,
    , bool2str(settings.debug),
    , bool2str(settings.serial)
  );

  return buffer;
}

char* getJSONState(uint8_t channel) {
  //Generate JSON 
  snprintf(buffer, BUF_SIZE, R"=="==({"channel":%.1i,"mode":"%.3s"}
)=="=="
    , channel
    , (channels[channel - 1] == MODE_ON) ? "on" : "off"
  );

  return buffer;
}


/**
 * Flash memory helpers 
 ********************************************************************************/

// CRC8 simple calculation
// Based on https://github.com/PaulStoffregen/OneWire/blob/master/OneWire.cpp
uint8_t crc8(const uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;

  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
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

void saveSettings(ST_SETTING &p_settings) {
  // Use the last byte for CRC
  uint8_t theCRC = crc8((uint8_t*) p_settings, sizeof(ST_SETTINGS));
  
  EEPROM.put(0, p_settings);
  /*
  for (int i = sizeof(buffer); i --> 0; ) {
    EEPROM.write(i, buffer[i]);
  }
  */
  EEPROM.update(sizeof(ST_SETTINGS), theCRC);
  EEPROM.commit();
}

int loadSettings(ST_SETTING &p_settings) {
  uint8_t const *buffer = (uint8_t*) p_settings;
  // Use the last byte for CRC
  
  EEPROM.get(0, p_settings)
  /*
  for (int i = sizeof(ST_SETTINGS); i --> 0; ) {
    buffer[i] = uint8_t(EEPROM.read(i));
  }
  */

  // Check CRC
  if (crc8(buffer, sizeof(ST_SETTINGS)) != uint8_t(EEPROM.read(i))) {
    logger.info("Bad CRC, loading default settings and waiting 10 seconds before writing to EEPROM.");
    setDefaultSettings(p_settings);
    delay(10000)
    saveSettings();
    return 0;
  }
  logger.setDebug(p_settings->debug);
  logger.setSerial(p_settings->serial);
  logger.info("Loaded settings from flash");

  // Display loaded setting on debug
  logger.debug("FLASH: %s", getJSONSettings());
  return 1;
}

void setDefaultSettings(ST_SETTING &p_settings) {
    strcpy(p_settings.login, DEFAULT_LOGIN);
    strcpy(p_settings.password, DEFAULT_PASSWORD);
    p_settings->debug = false;
    p_settings->serial = false;
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
  
  logger.info("Channel %.1i switched to %.3s", channel, (mode == MODE_ON) ? "on" : "off");
  logger.debug("Sending payload %02X%02X%02X%02X", payload[0], payload[1], payload[2], payload[3]);

  // Give some time to the watchdog
  ESP.wdtFeed();
  yield();

  // Send hex payload
  Serial.write(payload, sizeof(payload));

  if (settings.serial) {
    Serial.println(""); // Clear the line for log output
  }
}

void setup()  {
  WiFiManager wifiManager;
  
  Serial.begin(115200);
  EEPROM.begin(512);
  logger.info("RemoteRelay version %s started.", VERSION);
  
  // Load settigns from flash
  loadSettings(*settings);

  // Be sure the relay are in the default state (off)
  for (uint8_t i = sizeof(channels); i > 0; ) {
    setChannel(i, channels[--i]);
  }

  // Configure custom parameters
  WiFiManagerParameter http_login("htlogin", "HTTP Login", settings.login, AUTHBASIC_LEN);
  WiFiManagerParameter http_password("htpassword", "HTTP Password", settings.password, AUTHBASIC_LEN, "type='password'");
  wifiManager.setSaveConfigCallback([](){
    shouldSaveConfig = true;
  });
  wifiManager.addParameter(&http_login);
  wifiManager.addParameter(&http_password);
  
  // Connect to Wifi or ask for SSID
  wifiManager.autoConnect("RemoteRelay");

  // Save new configuration set by captive portal
  if (shouldSaveConfig) {
    strncpy(settings.login, http_login.getValue(), AUTHBASIC_LEN);
    strncpy(settings.password, http_password.getValue(), AUTHBASIC_LEN);

    logger.info("Saving new config from portal web page");
    saveSettings(*settings);
  }

  // Display local ip
  logger.info("Connected. IP address: %s", WiFi.localIP().toString().c_str());
  
  // Setup HTTP handlers
  server.on("/", handleGETRoot );
  server.on("/debug", HTTP_GET, handleGETDebug);
  server.on("/settings", HTTP_GET, handleGETSettings);
  server.on("/settings", HTTP_POST, handlePOSTSettings);
  server.on("/reset", HTTP_POST, handlePOSTReset);
  const char *_channelPath;
  for (uint8_t i = sizeof(channels) - 1; i > 0; i -= 2) {
    _channelPath = channelPath[i++];
    server.on(_channelPath, HTTP_PUT, std::bind(&handlePUTChannel, i));
    server.on(_channelPath, HTTP_GET, std::bind(&handleGETChannel, i));
  }
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found\r\n");
  });
  server.begin();
  
  logger.info("HTTP server started.");
}

// TODO: pucgenie: Handle the exact commands. Test button input.
void fakeATFirmware() {
  // pretend to be an AT device here
  if (Serial.available()) {
    String stringIn = Serial.readStringUntil('\r');
    Serial.flush(); // flush what's left '\n'?

    if (charnonempty(stringIn)) {
      logger.debug("Serial received: %s", stringIn);

      if (stringIn.indexOf("AT+") > -1) {
        Serial.println("OK");
      }

      if (stringIn.indexOf("AT+RST") > -1) {
        // pretend we reset (wait a bit then send the WiFi connected message)
        delay(1);
        Serial.println("WIFI CONNECTED\r\nWIFI GOT IP");
      }
    }
  }
}

void loop() {
    server.handleClient();
#ifdef NUVOTON_AT_REPLIES
    fakeATFirmware();
#endif
}
