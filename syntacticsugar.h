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

#ifndef SYNTACTICSUGAR_H
#define SYNTACTICSUGAR_H

#define bool2str(x) x ? "true" : "false"
#define charnonempty(x) x[0] != 0

/**
 * You can indirectly read the actual bit width (maximum value) from compiler warnings.
 * Using the slightly different approach mentioned in comments, w wouldn't get the compiler warning.
 * @author https://stackoverflow.com/users/179895/TripShock
 * https://stackoverflow.com/a/64862943/2714781
 */
#define GET_BIT_FIELD_WIDTH(T, f) \
    []() constexpr -> unsigned int \
    { \
        T t{}; \
        t.f = ~0; \
        unsigned int bitCount = 0; \
        while (t.f != 0) \
        { \
            t.f >>= 1; \
            ++bitCount; \
        } \
        return bitCount; \
    }()

#endif  // SYNTACTICSUGAR_H
