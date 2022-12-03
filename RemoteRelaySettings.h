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

#ifndef RemoteRelaySettings_H
#define RemoteRelaySettings_H

#include "Arduino.h"

#define AUTHBASIC_LEN_USERNAME 20        // Login or password 20 char max
#define AUTHBASIC_LEN_PASSWORD 20        // Login or password 20 char max
#define LENGTH_SSID 32
#define LENGTH_WPA_KEY 64

struct ST_SETTINGS_FLAGS {
  /**
    * Count the number of zeroes to rush along the linked list.
    * Setting a 1 to a 0 doesn't need to erase the sector (4kiB), or so I thought - depends on flash technology (NOR only?).
    * If it is full (all zeroes), it is not implemented to check those bits in following settings blocks.
    */
  // TODO: remove
  int16_t wearlevel_mark    :4 { ~0 };
  /**
    * Output debug messages
  **/
  int16_t debug             :1 { false };
  /**
    * Log output to serial port
  **/
  int16_t serial            :1 { false };
  /**
    * If set, webservice will be brought up on nuvoTon serial command or on boot if compiled with DISABLE_NUVOTON_AT_REPLIES:
      AT+CIPMUX=1
      AT+CIPSERVER=1,8080
      AT+CIPSTO=360
    *
    * If disabled, µC may sleep between ping pong intervals.
    */
  int16_t webservice        :1 { true };
  int16_t wifimanager_portal:1 { true };

  // TODO: move out of ST_SETTINGS_FLAGS
  uint16_t erase_cycles      :8 { 0 };
};

/**
 * This class provides access abstraction for EEPROM-backed settings storage.
 */
class RemoteRelaySettings {
  public:
    struct ST_SETTINGS_FLAGS flags;
    // ensure that no double-quotes get accepted for the login name so we don't have to escape for settings JSON serialization
    char login[AUTHBASIC_LEN_USERNAME+1];
    char password[AUTHBASIC_LEN_PASSWORD+1];
    /**
     * The access point's SSID.
     */
    char ssid[LENGTH_SSID+1];
    /**
     * The access point's password.
     */
    char wpa_key[LENGTH_WPA_KEY+1];
  
  private:
  
    ;
    
  public:
  
    bool loadSettings(uint16_t &the_address);
    void saveSettings(uint16_t &p_settings_offset);
    #ifdef EEPROM_SPI_NOR_REPROGRAM
    /**
    * write a zero anywhere in CRC to force loading default settings at boot
    */
    static void eeprom_destroy_crc(uint16_t old_addr);
    /**
     * CRC16 simple calculation
     * Based on CRC8 https://github.com/PaulStoffregen/OneWire/blob/master/OneWire.cpp
     * implementing polynomial 17 bits https://users.ece.cmu.edu/~koopman/crc/ 0x16FA7 >> 1
     * NOT IMPLEMENTED.
     * Rolled back to crc8.
    **/
    #endif
    static uint8_t crc8(const uint8_t *addr, size_t len);
  
  private:
  
    ;
    
};

#endif  // RemoteRelaySettings_H
