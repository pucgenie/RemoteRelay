/*************************************************************************
 *
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

#include "divideandconquer_01.h"

//#include "syntacticsugar.h"

bool DivideAndConquer01::binarysearchString(size_t &pivot, const String * const sortedList, const String &value, size_t upperBound) {
  //assert table != NULL && value != NULL
  size_t lowerBound = 0;
  pivot = upperBound / 2;
  while (lowerBound != upperBound) {
    const String &elem = sortedList[pivot];
    // TODO: pucgenie: what abstract data type does compareTo return??
    int diff = value.compareTo(elem);
    if (diff == 0) {
return true;
    }
    if (diff > 0) {
      lowerBound = pivot + 1;
    } else // assert if (diff < 0)
    {
      upperBound = pivot - 1;
    }
    pivot = lowerBound + (upperBound - lowerBound) / 2;
  }
  return false;
}

// non-DRY
bool DivideAndConquer01::binarysearchChars(size_t &pivot, const char * const * const sortedList, const char * const value, size_t upperBound, const size_t &max_str_len) {
  //assert table != NULL && value != NULL
  size_t lowerBound = 0;
  pivot = upperBound / 2;
  while (lowerBound != upperBound) {
    const char * const elem = sortedList[pivot];
    int diff = strncmp(value, elem, max_str_len);
    if (diff == 0) {
return true;
    }
    if (diff > 0) {
      lowerBound = pivot + 1;
    } else // assert if (diff < 0)
    {
      upperBound = pivot - 1;
    }
    pivot = lowerBound + (upperBound - lowerBound) / 2;
  }
  return false;
}
