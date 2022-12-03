/*************************************************************************
 *
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

#ifndef DIVIDEANDCONQUER01_H
#define DIVIDEANDCONQUER01_H

#include "Arduino.h"

class DivideAndConquer01 {
    public:
        bool binarysearchString(size_t &pivot, const String * const table, const String &value, size_t upperBound);
        bool binarysearchChars(size_t &pivot, const char * const * const table, const char * const value, size_t upperBound, const size_t max_str_len);
};
#endif  // DIVIDEANDCONQUER01_H
