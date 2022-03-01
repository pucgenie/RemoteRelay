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
 */

void ATReplies::handle_nuvoTon_comms(Logger &logger) {
  String stringIn = Serial.readStringUntil('\r');
  Serial.flush(); // flush what's left '\n'?

  if (charnonempty(stringIn)) {
    logger.debug(PSTR("Serial received: %s"), stringIn);

    if (stringIn.indexOf("AT+") > -1) {
      Serial.println("OK");
    }

    if (stringIn.indexOf("AT+RST") > -1) {
      // pretend we reset (wait a bit then send the WiFi connected message)
      delay(10);
      Serial.println(F("WIFI CONNECTED\r\nWIFI GOT IP"));
    }

    // Button2: AT+RESTORE und komplette Initialisierung
  }
}
