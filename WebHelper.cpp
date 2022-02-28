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
      && !server.authenticate(settings.login, settings.password)) {
    server.requestAuthentication();
return false;
  }
  return true;
}

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
  getJSONSettings(buffer, BUF_SIZE);
  server.send(200, F("application/json"), buffer);
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
  struct ST_SETTINGS_FLAGS isNew = { false, false, false, false };

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
        server.arg(i).toCharArray(settings.login, AUTHBASIC_LEN_USERNAME);
        isNew.login = true;
    break;
      case 2: // password
        server.arg(i).toCharArray(settings.password, AUTHBASIC_LEN_PASSWORD);
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
    logger.info(F("Updated debug to %.5s."), bool2str(settings.debug));
  }

  if (isNew.serial) {
    logger.setSerial(settings.serial);
    logger.info(F("Updated serial to %.5s."), bool2str(settings.serial));
  }

  if (isNew.login) {
    logger.info(F("Updated login to \"%s\"."), settings.login);
  }

  if (isNew.password) {
    logger.info(F("Updated password."));
  }

  saveSettings(settings);

  // Reply with current settings
  getJSONSettings(buffer, BUF_SIZE);
  server.send(201, F("application/json"), buffer);
}

/**
 * POST /reset
 */
void handlePOSTReset() {
  if (!isAuthBasicOK()) {
return;
  }
  
  logger.info(F("Reset settings to default"));

  resetWiFiManager();
  setDefaultSettings(settings);
  saveSettings(settings);
  
  // Send response now
  server.send(200, F("text/plain"), F("Reset OK"));
  
  delay(3000);
  logger.info(F("Restarting..."));
  
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
void handleGETChannel(uint8_t channel) {
  if (!isAuthBasicOK()) {
return;
  }

  server.send(200, F("application/json"), getJSONState(channel));
}

void setup_web_handlers(size_t channel_count) {
  // pucgenie: Don't use F() for map keys.
  server.on("/", handleGETRoot );
  server.on("/debug", HTTP_GET, handleGETDebug);
  server.on("/settings", HTTP_GET, handleGETSettings);
  server.on("/settings", HTTP_POST, handlePOSTSettings);
  server.on("/reset", HTTP_POST, handlePOSTReset);
  const char *_channelPath;
  while (channel_count --> 0) {
    _channelPath = channelPath[channel_count];
    server.on(_channelPath, HTTP_PUT, std::bind(&handlePUTChannel, channel_count+1));
    server.on(_channelPath, HTTP_GET, std::bind(&handleGETChannel, channel_count+1));
  }
  
  server.onNotFound([]() {
    server.send(404, F("text/plain"), F("Not found\r\n"));
  });
}
