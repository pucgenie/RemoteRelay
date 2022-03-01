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

#include "Logger.h"

static char buffer[BUF_LEN];

Logger::Logger() {
  // Init ring log
  for (int i = RINGLOG_SIZE; i --> 0; ) {
    ringlog[i][0]='\0';
  }

  enableDebug = false;
  enableSerial = false;
}

void Logger::setDebug(bool d) {
  enableDebug = d;
}

void Logger::setSerial(bool d) {
  enableSerial = d;
}

void Logger::debug(PGM_P fmt, ...) {
  if (!enableDebug) {
return;
  }
  va_list ap;
  va_start(ap, fmt);
  log(fmt, ap);
  va_end(ap);
}

void Logger::info(PGM_P fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log(fmt, ap);
  va_end(ap);
}

void Logger::log(PGM_P fmt, va_list ap) {
  // Generate log message (does not support float)
  vsnprintf_P(buffer, BUF_LEN, fmt, ap);

  // Add timestamp header
  uint32_t uptime = millis();
  // pucgenie: don't use F() here.
  snprintf(ringlog[index], BUF_LEN, "[%04d.%03d] %s", uptime / 1000, uptime % 1000, buffer);

  if (enableSerial) {
    Serial.println(ringlog[index]);
  }

  // Loop over at the begining of the ring
  if (++index >= RINGLOG_SIZE) {
    index = 0;
  }
}
  
String Logger::getLog() {
  // Get uptime
  char uptime[9];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hour = min / 60;
  // pucgenie: Don't use F() here.
  snprintf(uptime, sizeof(uptime), "%02d:%02d:%02d", hour, min % 60, sec % 60);

  // Generate header
  String msg = " ==== DEBUG LOG ====";
  msg += "\r\nChip ID: ";
  msg += ESP.getChipId();
  msg += "\r\nFree Heap: ";
  msg += ESP.getFreeHeap();
  msg += "\r\nFlash Size: ";
  msg += ESP.getFlashChipSize();
  msg += "\r\nUptime: ";
  msg += uptime;
  msg += "\r\nPrinting last ";
  msg += RINGLOG_SIZE;
  msg += " lines of the log:\r\n";

  // Get most recent half of the ring
  for (int i = index; i < RINGLOG_SIZE; ++i) {
    if (strlen(ringlog[i]) > 0) {
      msg += ringlog[i];
      msg += "\r\n";
    }
  }

  // Get older half of the ring
  for (int i = 0; i < index; ++i) {
    if (strlen(ringlog[i]) > 0) {
      msg += ringlog[i];
      msg += "\r\n";
    }
  }

  msg += " ==== END LOG ====\r\n";

  if (enableSerial) {
    Serial.print(msg);
  }

  return msg;
}
