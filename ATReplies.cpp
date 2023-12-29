/*************************************************************************
 *
 * This file is part of the Remoterelay Arduino sketch.
 * Copyleft 2017 Nicolas Agius <nicolas.agius@lps-it.fr>
 * Copyright 2021 https://github.com/crstrand (unknown license)
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

#include "ATReplies.h"
#include "Logger.h"
#include "divideandconquer_01.h"

#include "syntacticsugar.h"

/**
 * === CWMODE=2 is mode 1 (red led)
 * Let me name it SETUP MODE
 * Simple script:
   AT+CWMODE=2
   AT+CWMODE=2
   AT+RST
   AT+CIPMUX=1
   AT+CIPSERVER=1,8080
   AT+CIPSTO=360
 * 
 * === CWMODE=1 is mode 2 (blue led)
 * Let me name it CLIENT MODE
 * Awaits responses:
   AT+CWMODE=1
   AT+CWMODE=1
   AT+RST
 * Sends up to 3 AT+RST, interval ~ 28 sec
 * After timeout it sends
   AT+CWSTARTSMART
   AT+CWSMARTSTART=1
 * 
 * 
 * But if answering (regardless of timeout):
 * WIFI CONNECTED
 * WIFI GOT IP
 * it doesn't timeout and continues:
   AT+CIPMUX=1
   AT+CIPSERVER=1,8080
   AT+CIPSTO=360
 * 
 * 
 * === Button S2
 * Reset.
   AT+RESTORE
 * Then continues exactly like CWMODE=2 (and changes to red led)
 * 
 * 
 * === Other command strings found in original firmware
 * AT+CWJAP:%d
 * AT+CIPUPDATE:2
 * AT+CIPUPDATE:3
 * AT+CIPUPDATE:4
 * AT+GMR
 * AT+PING
 * AT+SLEEP
 * ...
 */



MyATCommand ATReplies::handle_nuvoTon_comms(Logger &logger) {
  // Let's hope that communication doesn't get interrupted and that it doesn't take too long.
  // TODO: Use SafeString library / asynchronous Serial reading?
  const String stringIn = Serial.readStringUntil('\n');
  const size_t strInLen = stringIn.length();
  if (strInLen == 0) {
    logger.logNow("{'error': 'empty line on serial encountered'}");
return INVALID_EXPECTED_AT;
  }
//  {
//    char tmpStr[strInLen + 1];
//    stringIn.toCharArray(tmpStr, strInLen);
//    tmpStr[strInLen] = '\0';
//    logger.debug(F("{'Serial_received': '%s'}"), tmpStr);
//  }
  if (!stringIn.startsWith("AT+")) {
    logger.debug(F("{'error': 'unexpected input', 'rawdata': '%s'}"), stringIn);
return INVALID_EXPECTED_AT;
  }

  #define GENERATE_STRING(STRING) #STRING,
  const String COMMAND_STRINGS[] = {
    MyATCommand_gen(GENERATE_STRING)
  };
  #undef GENERATE_STRING
  
  MyATCommand ret = INVALID_EXPECTED_AT;
  String partToSearch = stringIn.substring(3);
  // FIXME: split at next plus-sign
  if (DivideAndConquer01::binarysearchString(ret, COMMAND_STRINGS, partToSearch, sizeof(COMMAND_STRINGS))) {
return ret;
  } else {
return INVALID_EXPECTED_AT;
  }
}

inline void ATReplies::answer_ok(Logger &logger) {
  Serial.println("OK");
}
