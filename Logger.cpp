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

void Logger::debug(const __FlashStringHelper *fmt, ...) {
  if (!enableDebug) {
return;
  }
  va_list ap;
  va_start(ap, fmt);
  log(fmt, ap);
  va_end(ap);
}

void Logger::info(const __FlashStringHelper *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log(fmt, ap);
  va_end(ap);
}


void Logger::logNow(const char* p_buffer) {
  // Add timestamp header
  uint32_t uptime = millis();
  // pucgenie: don't use F() here.
  // Standard compilers recognize DIV and modulo and optimize if supported by hardware.
  snprintf(ringlog[index], BUF_LEN, "[%04d.%03d] ", uptime / 1000, uptime % 1000);
  strncpy(ringlog[index] + 11, p_buffer, BUF_LEN - 11);
  
  if (enableSerial) {
    Serial.println(ringlog[index]);
  }
  
  // Loop over at the begining of the ring
  if (++index >= RINGLOG_SIZE) {
    index = 0;
  }
}

void Logger::log(const __FlashStringHelper *fmt, va_list ap) {
  // just keep it allocated
  static char buffer[BUF_LEN];
  // Generate log message (does not support float)
  // TODO: Handle return code.
  vsnprintf_P(buffer, BUF_LEN, reinterpret_cast<PGM_P>(fmt), ap);
  logNow(buffer);
}

void Logger::getLog(String &msg) {
  // Get uptime
  char uptime[9];
  // pucgenie: microoptimization: Don't use F() here.
  // TODO: Handle return code.
  if (snprintf(uptime, sizeof(uptime), "%08d", millis() / 1000) >= sizeof(uptime)) {
    // can't use logger...
    Serial.println(F("Time formatting broken. Continuing anyway..."));
  }

  // pucgenie: Don't worry about a few wasted CPU cycles to do that everytime.
  if (!msg.reserve(255)) {
    // can't use logger...
    Serial.println(F("Couldn't reserve String buffer. Continuing anyway..."));
  }
  // Generate header
  msg += " ==== DEBUG LOG ====";
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
}
