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

#include "divideandconquer.h"

//#include "syntacticsugar.h"

uint8_t binarysearchString(const String *table, const String value) {
  //assert table != NULL && value != NULL
  uint8_t lowerBound = 0, upperBound = sizeof(table);
  uint8_t pivot = upperBound / 2;
  while (lowerBound != upperBound) {
    String elem = table[pivot];
    // TODO: pucgenie: what data type does compareTo return??
    int diff = value.compareTo(elem);
    if (diff == 0) {
return pivot;
    }
    if (diff > 0) {
      lowerBound = pivot + 1;
    } else // assert if (diff < 0)
    {
      upperBound = pivot - 1;
    }
    pivot = lowerBound + (upperBound - lowerBound) / 2;
  }
  return -pivot - 1;
}
