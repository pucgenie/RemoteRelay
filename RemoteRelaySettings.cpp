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

#include "RemoteRelaySettings.h"
#include "syntacticsugar.h"
#include "ledsignalling.h"
#include "RemoteRelay.h"

extern "C" {
#include <spi_flash.h>
}
// modded to support 256 Byte page writes 16 times in one 4K erase sector
#include "EEPROM.h"

#include "Logger.h"

// object size plus crc8. TODO: Check if already aligned to 256 Bytes (page size)
#define SETTINGS_FLASH_SIZE sizeof(RemoteRelaySettings)+1
#define SETTINGS_FLASH_OVERADDR FLASH_SECTOR_SIZE - (SETTINGS_FLASH_SIZE)
#define SETTINGS_FLASH_WEARLEVEL_MARK_BITS GET_BIT_FIELD_WIDTH(ST_SETTINGS_FLAGS, wearlevel_mark)

/**
 * Reads settings from EEPROM flash into this object.
 * Returns the byte start location of the loaded settings block.
 * 
 * @param the_address Should be 0 to load the first valid settings block. Can be an exact address too.
 */
bool RemoteRelaySettings::loadSettings(uint16_t &the_address) {
  bool ret;
  while (ret = (the_address < (SETTINGS_FLASH_OVERADDR))) {
    EEPROM.get(the_address, *this);
    // check if marked as deleted and how many bits are set 0
    const int x = this->flags.wearlevel_mark;
    if (x < ((1<<SETTINGS_FLASH_WEARLEVEL_MARK_BITS) - 1)) {
      #pragma clang loop unroll(full)
      //#pragma GCC unroll 8
      for (int nb = SETTINGS_FLASH_WEARLEVEL_MARK_BITS; nb --> 0; ) {
        // some way to spare one instruction?^^
        if ((x & (1<<nb)) == 0) {
          the_address += SETTINGS_FLASH_SIZE;
        }
      }
    } else if (crc8((uint8_t*) this, sizeof(RemoteRelaySettings)) == uint8_t(EEPROM.read(the_address + sizeof(RemoteRelaySettings)))) {
      // index of valid settings found
      EEPROM.get(the_address, *this);
  break;
    } else {
      the_address += SETTINGS_FLASH_SIZE;
    }
  }
  if (!ret) {
    logger.info(F("{'settings': 'loading default'}"));
    //setDefaultSettings(*this);
    strncpy_P(this->login, PSTR(DEFAULT_LOGIN), AUTHBASIC_LEN_USERNAME+1);
    strncpy_P(this->password, PSTR(DEFAULT_PASSWORD), AUTHBASIC_LEN_PASSWORD+1);
    strncpy_P(this->ssid, PSTR(DEFAULT_STANDALONE_SSID), LENGTH_SSID+1);
    strncpy_P(this->wpa_key, PSTR(DEFAULT_STANDALONE_WPA_KEY), LENGTH_WPA_KEY+1);
    this->flags.wearlevel_mark = ~0;
    this->flags.erase_cycles = 0;
    this->flags.debug = false;
    this->flags.serial = false;
    this->flags.wifimanager_portal = true;
    this->flags.webservice = true;
    
    led_scream(0b10101010);
    
    the_address = 0;
  } else {
    // serial is disabled by default, so spare us another if after setting
    logger.info(F("{'settings': 'loaded from flash'}"));
  }
  // could have changed
  logger.setSerial(this->flags.serial);

  // Display loaded setting on debug
  if (this->flags.debug) {
    char buffer[BUF_SIZE];
    if (this->getJSONSettings(buffer, BUF_SIZE)) {
      
    }
    logger.logNow(buffer);
  }
  return ret;
}

void RemoteRelaySettings::saveSettings(uint16_t &p_settings_offset) {
  #ifdef EEPROM_SPI_NOR_REPROGRAM
  {
    uint16_t old_addr = p_settings_offset;
    RemoteRelaySettings::eeprom_destroy_crc(old_addr);
  }
  #endif
  uint8_t theCRC = crc8((uint8_t*) this, sizeof(RemoteRelaySettings));
  p_settings_offset += SETTINGS_FLASH_SIZE;
  if (p_settings_offset >= (SETTINGS_FLASH_OVERADDR - SETTINGS_FLASH_SIZE)) {
    this->flags.erase_cycles += 1;
    // check wrap-around / overflow
    if (this->flags.erase_cycles == 0) {
      logger.info(F("{'settings': 'more than 256 erase cycles of settings block reached'}"));
    }
    p_settings_offset = 0;
    //TODO: erase sector explicitly or keep using EEPROM class' auto-detection?
  }
  EEPROM.put(p_settings_offset, *this);
  EEPROM.put(p_settings_offset + sizeof(RemoteRelaySettings), theCRC);
  EEPROM.commit();
}

size_t RemoteRelaySettings::getJSONSettings(char * const p_buffer, const size_t bufSize) {
  //Generate JSON 
  const size_t snstatus = snprintf_P(p_buffer, bufSize, LOWMEMORY_STR(R"=="==({"login":"%s","debug":%.5s,"serial":%.5s,"webservice":%.5s,"wifimanager_portal":%.5s}
)=="==")
    , this->login
    , bool2str(this->flags.debug)
    , bool2str(this->flags.serial)
    , bool2str(this->flags.webservice)
    , bool2str(this->flags.wifimanager_portal)
  );
  assert(snstatus > 0 && snstatus < bufSize);
  return snstatus;
}

#undef SETTINGS_FLASH_OVERADDR
#undef SETTINGS_FLASH_SIZE
#undef SETTINGS_FLASH_WEARLEVEL_MARK_BITS

uint8_t RemoteRelaySettings::crc8(const uint8_t *addr, size_t len) {
  uint8_t crc = 0;

  while (len--) {
    uint8_t inbyte = *(addr++);
    #pragma clang loop unroll(full)
    #pragma GCC unroll 8
    for (uint8_t i = 8; i --> 0;) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) {
        crc ^= 0b10001100;
      }
      inbyte >>= 1;
    }
  }
  return crc;
}
#ifdef EEPROM_SPI_NOR_REPROGRAM
void RemoteRelaySettings::eeprom_destroy_crc(const uint16_t &old_addr) {
  RemoteRelaySettings tmp_settings();
  if (!tmp_settings.loadSettings(old_addr)) {
    // it already is incorrect - nop.
return;
  }
  tmp_settings.flags.wearlevel_mark <<= 1;
  uint8_t crc = EEPROM.read(old_addr + sizeof(RemoteRelaySettings));
  if (crc == crc8((uint8_t*) &tmp_settings, sizeof(RemoteRelaySettings))) {
    if (crc == 0) {
      // what a coincidence
      bool found = false;
      if (offsetof(RemoteRelaySettings, password) < offsetof(RemoteRelaySettings, login)) {
        // FIXME: need a define for char[] capacities in RemoteRelaySettings...
        while (true) {
          led_scream(0b11001100);
        }
        // # error ERROR RemoteRelaySettings members changed unpredictably
      }
      static const uint16_t OVERWRITABLE_BYTES[] = {
        offsetof(RemoteRelaySettings, password) - offsetof(RemoteRelaySettings, login),
        offsetof(RemoteRelaySettings, ssid) - offsetof(RemoteRelaySettings, password),
        offsetof(RemoteRelaySettings, wpa_key) - offsetof(RemoteRelaySettings, ssid),
        sizeof(((RemoteRelaySettings *)0)->wpa_key)
      };
      bool string_terminator_found;
      byte overwritable_field_index = sizeof(OVERWRITABLE_BYTES);
      char *ptr = tmp_settings.login;
      while ((!found) && (overwritable_field_index --> 0)) {
        string_terminator_found = false;
        // pucgenie: I wonder whether or not the compiler sees
        //#pragma clang loop unroll_count(16)
        //#pragma GCC unroll 16
        for (int i = OVERWRITABLE_BYTES[overwritable_field_index]; i --> 0; ++ptr) {
          if ((*ptr) == 0) {
            string_terminator_found = true;
          } else if (string_terminator_found) {
            (*ptr) = 0;
            // We are certain that changing a single bit changes the resulting CRC too.
            found = true;
        break;
            }
        }
      }
      // don't touch RemoteRelaySettings.flags.erase_cycles
      if (!found) {
        // FIXME: erase
        while (true) {
          led_scream(0b11101110);
        }
      }
    } else {
      // Arduino.h byte is unsigned
      byte i = 0b10000000;
      while (i > 0 && (crc & i) == 0) {
        i >>= 1;
      }
      //assert i != 0;
      crc ^= i;
      // it can't possibly happen that it doesn't find a bit because 0 case was handled before.
      EEPROM.write(old_addr + sizeof(RemoteRelaySettings), crc);
    }
    EEPROM.put(old_addr, tmp_settings);
    // don't commit
  }
}
#endif
