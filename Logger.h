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

#ifndef LOGGER_H
#define LOGGER_H

#include "Arduino.h"

#define BUF_LEN 180           // Max length of each line of log
#define RINGLOG_SIZE 100      // Max number of line in the ring log

/**
 * This class provide a logging facility to print log messages on the 
 * serial output and store the last ones in a ring buffer for further access.
 */
class Logger {
  private:
  
    char ringlog[RINGLOG_SIZE][BUF_LEN];
    int index;
    bool enableDebug;
    bool enableSerial;
    
  public:
  
    Logger();
    
    void info(PGM_P, ...);     // Print and store message log
    void debug(PGM_P, ...);    // Print and store message log if debug mode is enabled
    void setSerial(bool);             // Enable log output on serial port
    void setDebug(bool);              // Enable debug log output
    String getLog(void);              // Return the current log
  
  private:
  
    void log(PGM_P, va_list);
    
};

#endif  // LOGGER_H
