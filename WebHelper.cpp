/*************************************************************************
 *
 * This file is part of the Remoterelay Arduino sketch.
 * Copyleft 2017 Nicolas Agius <nicolas.agius@lps-it.fr>
 * Copyleft 2023 Johannes Unger (just minor enhancements)
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

// FlashStringHelper
#include <Arduino.h>

#include "RemoteRelay.h"
#include "syntacticsugar.h"

#include "divideandconquer_01.h"

static const char CT_JSON[] = "application/json";
static const char CT_TEXT[] = "text/plain";

// define enum stringlist https://stackoverflow.com/a/10966395
#define FOREACH_FRUIT1(FRUIT)      \
        FRUIT(debug)              \
        FRUIT(login)              \
        FRUIT(password)           \
        FRUIT(serial)             \
        FRUIT(ssid)               \
        FRUIT(webservice)         \
        FRUIT(wifimanager_portal) \
        FRUIT(wpa_key)            \

#define GENERATE_ENUM(ENUM) WEB_PARAM_##ENUM,

enum ENUM_WEB_PARAM {
    FOREACH_FRUIT1(GENERATE_ENUM)
};

#undef GENERATE_ENUM
#define GENERATE_STRING(STRING) #STRING,

// pucgenie: Is the overhead relevant?
static const String WEB_PARAM[] = {
    FOREACH_FRUIT1(GENERATE_STRING)
};

#undef GENERATE_STRING
#undef FOREACH_FRUIT1

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
  String fromLog;
  logger.getLog(fromLog);
  wifiManager.server->send(200, CT_TEXT, fromLog);
  // pucgenie: Note to myself: In C++ a stack object's destructor is called automatically.
  //delete &fromLog;
}

/**
 * GET /settings
 */
void handleGETSettings() {
  if (!isAuthBasicOK()) {
return;
  }
  // stack, no fragmentation
  char buffer[BUF_SIZE];
  if (settings.getJSONSettings(buffer, BUF_SIZE) > BUF_SIZE) {
    // TODO: tell that something broke
  }
  wifiManager.server->send(200, CT_JSON, String(buffer));
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
    wifiManager.server->send(400, CT_TEXT, F("Invalid parameters\r\n"));
return;
  }

  // Parse args   
  for (uint8_t i = wifiManager.server->args(); i --> 0; ) {
    const String param = wifiManager.server->argName(i);
    size_t idxOut;
    if (!DivideAndConquer01::binarysearchString(idxOut, WEB_PARAM, param, sizeof(WEB_PARAM))) {
      wifiManager.server->send(400, CT_TEXT, "Unknown parameter: " + param + "\r\n");
return;
    }
    switch ((ENUM_WEB_PARAM) idxOut) {
      default: {
        wifiManager.server->send(400, CT_TEXT, "Unimplemented parameter: " + param + "\r\n");
return;
      }
      case WEB_PARAM_debug: { // debug
        settings.flags.debug = wifiManager.server->arg(i).equalsIgnoreCase("true");
        logger.info(F("{'updated_debug': %.5s}"), bool2str(settings.flags.debug));
      }
    break;
      case WEB_PARAM_login: { // login
        wifiManager.server->arg(i).toCharArray(settings.login, AUTHBASIC_LEN_USERNAME);
        logger.info(F("{'updated_login': '%s}"), settings.login);
      }
    break;
      case WEB_PARAM_password: { // password
        wifiManager.server->arg(i).toCharArray(settings.password, AUTHBASIC_LEN_PASSWORD);
        logger.info(F("{'updated_password': '%s'}"), settings.password);
      }
    break;
      case WEB_PARAM_serial: { // serial
        settings.flags.serial = wifiManager.server->arg(i).equalsIgnoreCase("true");
        logger.setSerial(settings.flags.serial);
        logger.info(F("{'updated_serial': %.5s}"), bool2str(settings.flags.serial));
      }
    break;
      case WEB_PARAM_wifimanager_portal: { // 
        bool newSetting = wifiManager.server->arg(i).equalsIgnoreCase("true");
        if (settings.flags.wifimanager_portal != newSetting) {
          // FIXME: stop or start it
        }
        settings.flags.wifimanager_portal = newSetting;
        logger.info(F("{'updated_wifimanager_portal': %.5s}"), bool2str(settings.flags.wifimanager_portal));
      }
    break;
      case WEB_PARAM_webservice: { // 
        bool newSetting = wifiManager.server->arg(i).equalsIgnoreCase("true");
        if (settings.flags.webservice != newSetting) {
          // FIXME: stop or start it
        }
        settings.flags.webservice = newSetting;
        logger.info(F("{'updated_webservice': %.5s}"), bool2str(settings.flags.webservice));
      }
    break;
      case WEB_PARAM_ssid: { // 
        wifiManager.server->arg(i).toCharArray(settings.ssid, AUTHBASIC_LEN_PASSWORD);
        logger.info(F("{'updated_serial': %.5s}"), bool2str(settings.flags.serial));
      }
    break;
      case WEB_PARAM_wpa_key: { // 
        settings.flags.serial = wifiManager.server->arg(i).equalsIgnoreCase("true");
        logger.setSerial(settings.flags.serial);
        logger.info(F("{'updated_serial': %.5s}"), bool2str(settings.flags.serial));
      }
    break;
    }
  }

  myLoopState = SAVE_SETTINGS;

  // Reply with current settings
  // stack, no fragmentation
  char buffer[BUF_SIZE];
  if (settings.getJSONSettings(buffer, BUF_SIZE) > BUF_SIZE) {
    // TODO: tell that something broke
  }
  wifiManager.server->send(201, CT_JSON, String(buffer));
}

/**
 * POST /reset
 */
void handlePOSTReset() {
  if (!isAuthBasicOK()) {
return;
  }
  
  logger.info(F("{'action': 'reset settings'}"));

  wifiManager.resetSettings();
  //setDefaultSettings(settings);

  // Don't write default settings in EEPROM flash...
  //saveSettings(settings);
  
  // Send response now
  wifiManager.server->send(200, CT_TEXT, F("Reset OK"));

  myLoopState = EEPROM_DESTROY_CRC;
}

/**
 * PUT /channel/:id
 * Args :
 *   - mode = "<on|off>"
 */
void handlePUTChannel(const uint8_t channel) {
  // TODO: pucgenie: Can't server handle this check itself?
  if (!isAuthBasicOK()) {
return;
  }
  
  // Check if args have been supplied
  // Check if requested arg has been suplied
  if (wifiManager.server->args() != 1 || wifiManager.server->argName(0) != "mode") {
    wifiManager.server->send(400, CT_JSON, F("{'invalidParameter': 'mode expected'}"));
return;
  }

  RSTM32MODE requestedMode;
  const String value = wifiManager.server->arg(0);
  if (value.equalsIgnoreCase("on")) {
    requestedMode = R_CLOSE;
  } else if (value.equalsIgnoreCase("off")) {
    requestedMode = R_OPEN;
  } else {
    // pucgenie: Could have used a format string instead, but look how much pre-parsing and pre-compiling I did manually. xP
    String msg(); //RAII
    /*static (... just put it on the stack using array-initializer) */ const char msgTail[] = ", 'expected': ['on', 'off']}";
    const char msgHead[] = "{'invalid': ";
    msg.reserve(sizeof(msgHead) + value.length() + sizeof(msgTail));
    msg.concat(msgHead);
    msg.concat(value);
    msg.concat(msgTail);
    wifiManager.server->send(400, CT_JSON, msg);
return;
  } 

  // Give some time to the watchdog
  ESP.wdtFeed();
  yield();

  setChannel(channel, requestedMode);
  // stack, no fragmentation
  handleGETSettings();
}

/**
 * GET /channel/:id
 */
void handleGETChannel(const uint8_t channel) {
  if (!isAuthBasicOK()) {
return;
  }
  // stack, no fragmentation
  char buffer[BUF_SIZE];
  getJSONState(channel, buffer, BUF_SIZE);
  wifiManager.server->send(200, CT_JSON, String(buffer));
}

void setup_web_handlers(size_t channel_count) {
  // pucgenie: Don't use F() for map keys.

  // keep default portal
  //wifiManager.server->on("/", handleGETRoot );
  
  wifiManager.server->on("/debug", HTTP_GET, handleGETDebug);
  wifiManager.server->on("/settings", HTTP_GET, handleGETSettings);
  wifiManager.server->on("/settings", HTTP_POST, handlePOSTSettings);
  wifiManager.server->on("/reset", HTTP_POST, handlePOSTReset);
  char _channelPath[] = "/channel/#";
  do {
    _channelPath[sizeof(_channelPath) / sizeof(_channelPath[0]) - 2] = '0' + channel_count;
    // TODO: Check if the library copies the string
    wifiManager.server->on(_channelPath, HTTP_PUT, std::bind(&handlePUTChannel, channel_count));
    wifiManager.server->on(_channelPath, HTTP_GET, std::bind(&handleGETChannel, channel_count));
  } while (channel_count-- != 0);
  /* wifiManager can do better.
  wifiManager.server->onNotFound([]() {
    wifiManager.server->send(404, CT_TEXT, F("Not found\r\n"));
  });
  */
}
