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

#ifndef ATREPLIES_H
#define ATREPLIES_H

#include <Arduino.h>
#include "Logger.h"

// define enum stringlist https://stackoverflow.com/a/10966395
#define MyATCommand_gen(FRUIT)      \
        FRUIT(RESTORE)              \
        FRUIT(RST)              \
        FRUIT(CWMODE_1)           \
        FRUIT(CWMODE_2)             \
        FRUIT(CWSTARTSMART)               \
        FRUIT(CWSMARTSTART_1)         \
        FRUIT(CIPMUX_1) \
        FRUIT(CIPSERVER)            \
        FRUIT(CIPSTO)            \

#define GENERATE_ENUM(ENUM) ##ENUM,
enum MyATCommand {
    MyATCommand_gen(GENERATE_ENUM)
    INVALID_EXPECTED_AT,
};
#undef GENERATE_ENUM

// TODO: pucgenie: Handle the exact commands.
class ATReplies {
  private:
    static int cwmode;
//    static const char* COMMAND_STRINGS[];
    
  public:
    static MyATCommand handle_nuvoTon_comms(Logger &logger);
    static void answer_ok(Logger &logger);
    
};

#endif  // ATREPLIES_H
