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

#include "WebHelper.h"

#include "RemoteRelay.h"
#include "syntacticsugar.h"

#include "divideandconquer.h"

static PGM_P CT_TEXT PROGMEM = "text/plain";
static PGM_P CT_JSON PROGMEM = "application/json";

// pucgenie: Don't use F() here
static const String WebParam[] = {
    "debug"
  , "login"
  , "password"
  , "serial"
  , "webservice"
};

// pucgenie: We need the memory in any case because there is no path tree parser...
// pucgenie: Don't use F() here.
static const char *channelPath[] = {
    "/channel/1"
  , "/channel/2"
#ifdef FOUR_WAY_MODE
  , "/channel/3"
  , "/channel/4"
#endif
                      };

bool isAuthBasicOK() {
  // Disable auth if not credential provided
  if (charnonempty(settings.login) && charnonempty(settings.password)
      && !wifiManager.server->authenticate(settings.login, settings.password)) {
    wifiManager.server->requestAuthentication();
return false;
  }
  return true;
}

/**
 * HTTP route handlers
 *******************************************************************************
 */

/**
 * GET /debug
 */
void handleGETDebug() {
  if (!isAuthBasicOK()) {
return;
  }
 
  wifiManager.server->send(200, FPSTR(CT_TEXT), logger.getLog());
}

/**
 * GET /settings
 */
void handleGETSettings() {
  if (!isAuthBasicOK()) {
return;
  }
  getJSONSettings(buffer, BUF_SIZE);
  wifiManager.server->send(200, FPSTR(CT_JSON), buffer);
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
  if (wifiManager.server->args() == 0) {
    wifiManager.server->send(400, FPSTR(CT_TEXT), F("Invalid parameters\r\n"));
return;
  }

  // Parse args   
  for (uint8_t i = wifiManager.server->args(); i --> 0; ) {
    String param = wifiManager.server->argName(i);
    switch (binarysearchString(WebParam, param)) {
      default:
        wifiManager.server->send(400, FPSTR(CT_TEXT), "Unknown parameter: " + param + "\r\n");
return;
      case 0: // debug
        settings.flags.debug = wifiManager.server->arg(i).equalsIgnoreCase("true");
        logger.info(PSTR("Updated debug to %.5s."), bool2str(settings.flags.debug));
    break;
      case 1: // login
        wifiManager.server->arg(i).toCharArray(settings.login, AUTHBASIC_LEN_USERNAME);
        logger.info(PSTR("Updated login to \"%s\"."), settings.login);
    break;
      case 2: // password
        wifiManager.server->arg(i).toCharArray(settings.password, AUTHBASIC_LEN_PASSWORD);
        logger.info(PSTR("Updated password."));
    break;
      case 3: // serial
        settings.flags.serial = wifiManager.server->arg(i).equalsIgnoreCase("true");
        logger.setSerial(settings.flags.serial);
        logger.info(PSTR("Updated serial to %.5s."), bool2str(settings.flags.serial));
    break;
    }
  }

  saveSettings(settings);

  // Reply with current settings
  getJSONSettings(buffer, BUF_SIZE);
  wifiManager.server->send(201, FPSTR(CT_JSON), buffer);
}

/**
 * POST /reset
 */
void handlePOSTReset() {
  if (!isAuthBasicOK()) {
return;
  }
  
  logger.info(PSTR("Reset settings to default"));

  wifiManager.resetSettings();
  //setDefaultSettings(settings);

  // Don't write default settings in EEPROM flash...
  //saveSettings(settings);
  
  eeprom_destroy_crc();
  // Send response now
  wifiManager.server->send(200, FPSTR(CT_TEXT), F("Reset OK"));

  //myLoopState = EEPROM_DESTROY_CRC;
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
  if (wifiManager.server->args() != 1 || wifiManager.server->argName(0) != "mode") {
    wifiManager.server->send(400, FPSTR(CT_TEXT), F("Invalid parameter\r\n"));
return;
  }

  uint8_t requestedMode = MODE_OFF; // Default in case of error
  String value = wifiManager.server->arg(0);
  if (value.equalsIgnoreCase("on")) {
    requestedMode = MODE_ON;
  } else if (value.equalsIgnoreCase("off")) {
    requestedMode = MODE_OFF;
  } else {
    wifiManager.server->send(400, FPSTR(CT_TEXT), "Invalid value: " + value + "\r\n");
return;
  } 

  // Give some time to the watchdog
  ESP.wdtFeed();
  yield();

  setChannel(channel, requestedMode);
  getJSONState(channel, buffer, BUF_SIZE);
  wifiManager.server->send(200, FPSTR(CT_JSON), buffer);
}

/**
 * GET /channel/:id
 */
void handleGETChannel(uint8_t channel) {
  if (!isAuthBasicOK()) {
return;
  }
  getJSONState(channel, buffer, BUF_SIZE);
  wifiManager.server->send(200, FPSTR(CT_JSON), buffer);
}

void setup_web_handlers(size_t channel_count) {
  // pucgenie: Don't use F() for map keys.

  // keep default portal
  //wifiManager.server->on("/", handleGETRoot );
  
  wifiManager.server->on("/debug", HTTP_GET, handleGETDebug);
  wifiManager.server->on("/settings", HTTP_GET, handleGETSettings);
  wifiManager.server->on("/settings", HTTP_POST, handlePOSTSettings);
  wifiManager.server->on("/reset", HTTP_POST, handlePOSTReset);
  const char *_channelPath;
  while (channel_count --> 0) {
    _channelPath = channelPath[channel_count];
    wifiManager.server->on(_channelPath, HTTP_PUT, std::bind(&handlePUTChannel, channel_count+1));
    wifiManager.server->on(_channelPath, HTTP_GET, std::bind(&handleGETChannel, channel_count+1));
  }
  /* wifiManager can do better.
  wifiManager.server->onNotFound([]() {
    wifiManager.server->send(404, FPSTR(CT_TEXT), F("Not found\r\n"));
  });
  */
}
