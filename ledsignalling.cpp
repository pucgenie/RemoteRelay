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

#include "ledsignalling.h"

//#include "syntacticsugar.h"

#define LED_EIN() digitalWrite(LED_BUILTIN, LOW)
#define LED_AUS() digitalWrite(LED_BUILTIN, HIGH)

/**
 * multiplied by 32.
 */
static const uint8_t alternate_delays[] = {
  // intro delay
  32,
  // identification
  3, 3, 3, 9, 3, 9,
  // terminator
  0
};

/**
 * pass-by-value
 */
void led_scream(uint8_t value) {
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);
  const uint8_t *ptr = alternate_delays;
  bool state = false;
  while ((*ptr) != '\0') {
    delay(*(ptr++) * 32);
    state = !state;
    digitalWrite(LED_BUILTIN, state ? LOW : HIGH);
  }

  for (int8_t i = 8; i --> 0; value <<= 1) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(64);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(64);
    if (value & 128) {
      digitalWrite(LED_BUILTIN, LOW);
    }
    delay(64);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(96);
  }
}
