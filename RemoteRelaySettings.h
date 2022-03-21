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

union U_SETTINGS_FLAGS {
  struct {
    /**
      * Count the number of zeroes to rush along the linked list.
      * Remember: Setting a 1 to a 0 doesn't need to erase the sector (4kiB).
      * If it is full (all zeroes), it is not implemented to check those bits in following settings blocks.
      */
    int16_t wearlevel_mark    :4;
    /**
      * Output debug messages
    **/
    int16_t debug             :1;
    /**
      * Log output to serial port
    **/
    int16_t serial            :1;
    /**
      * If set, webservice will be brought up on nuvoTon serial command or on boot if compiled with DISABLE_NUVOTON_AT_REPLIES:
        AT+CIPMUX=1
        AT+CIPSERVER=1,8080
        AT+CIPSTO=360
      *
      * If disabled, µC may sleep between ping pong intervals.
      */
    int16_t webservice        :1;
    int16_t wifimanager_portal:1;

    int16_t erase_cycles      :8;
  };
  // each member needs to have the same type that the full bitfield has
  int16_t reg;
};

/**
 * This class provides access abstraction for EEPROM-backed settings storage.
 */
class RemoteRelaySettings {
  public:
    union U_SETTINGS_FLAGS flags;
    
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
  
    RemoteRelaySettings();
    bool loadSettings(uint16_t &the_address);
    void saveSettings(uint16_t &p_settings_offset);
    /**
    * write a zero anywhere in CRC to force loading default settings at boot
    */
    static void eeprom_destroy_crc(uint16_t &old_addr);
    /**
     * CRC8 simple calculation
     * Based on https://github.com/PaulStoffregen/OneWire/blob/master/OneWire.cpp
    **/
    static uint8_t crc8(const uint8_t *addr, uint8_t len);
  
  private:
  
    ;
    
};

#endif  // RemoteRelaySettings_H
