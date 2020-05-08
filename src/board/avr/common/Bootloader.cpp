/*

Copyright 2015-2020 Igor Petrovic

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "board/Board.h"
#include "board/Internal.h"
#include "core/src/general/Reset.h"
#include "core/src/general/Helpers.h"
#include "core/src/arch/avr/Misc.h"
#include "core/src/general/Interrupt.h"
#include "core/src/general/Timing.h"
#include "common/OpenDeckMIDIformat/OpenDeckMIDIformat.h"

namespace Board
{
#ifdef FW_BOOT
    namespace bootloader
    {
        size_t pageSize(size_t index)
        {
            return SPM_PAGESIZE;
        }

        void erasePage(size_t index)
        {
            boot_page_erase(index * pageSize(index));
            boot_spm_busy_wait();
        }

        void fillPage(size_t index, uint32_t address, uint16_t data)
        {
            boot_page_fill(address + (index * pageSize(index)), data);
        }

        void writePage(size_t index)
        {
            //write the filled FLASH page to memory
            boot_page_write(index * pageSize(index));
            boot_spm_busy_wait();

            //re-enable RWW section
            boot_rww_enable();
        }

        void applyFw()
        {
#ifdef LED_INDICATORS
            detail::io::ledFlashStartup(true);
#endif
            core::reset::mcuReset();
        }
    }    // namespace bootloader
#endif

    namespace detail
    {
        namespace bootloader
        {
            bool isAppValid()
            {
                return (pgm_read_word(0) != 0xFFFF);
            }

            bool isSWtriggerActive()
            {
                return eeprom_read_byte((uint8_t*)REBOOT_VALUE_EEPROM_LOCATION) == BTLDR_REBOOT_VALUE;
            }

            void enableSWtrigger()
            {
                eeprom_write_byte((uint8_t*)REBOOT_VALUE_EEPROM_LOCATION, BTLDR_REBOOT_VALUE);
            }

            void clearSWtrigger()
            {
                eeprom_write_byte((uint8_t*)REBOOT_VALUE_EEPROM_LOCATION, APP_REBOOT_VALUE);
            }

            void runApplication()
            {
                __asm__ __volatile__(
                    // Jump to RST vector
                    "clr r30\n"
                    "clr r31\n"
                    "ijmp\n");
            }

            void runBootloader()
            {
                //relocate the interrupt vector table to the bootloader section
                MCUCR = (1 << IVCE);
                MCUCR = (1 << IVSEL);

                ENABLE_INTERRUPTS();

                detail::bootloader::indicate();

#if defined(USB_LINK_MCU) || !defined(USB_MIDI_SUPPORTED)
                Board::UART::init(UART_USB_LINK_CHANNEL, UART_BAUDRATE_MIDI_OD);

#ifndef USB_MIDI_SUPPORTED
                // make sure USB link goes to bootloader mode as well
                MIDI::USBMIDIpacket_t packet;

                packet.Event = static_cast<uint8_t>(OpenDeckMIDIformat::command_t::btldrReboot);
                packet.Data1 = 0x00;
                packet.Data2 = 0x00;
                packet.Data3 = 0x00;

                //add some delay - it case of hardware btldr entry both MCUs will boot up in the same time
                //so it's possible USB link MCU will miss this packet

                core::timing::waitMs(1000);

                OpenDeckMIDIformat::write(UART_USB_LINK_CHANNEL, packet, OpenDeckMIDIformat::packetType_t::internalCommand);
#endif
#endif

#ifdef USB_MIDI_SUPPORTED
                detail::setup::usb();
#endif
            }
        }    // namespace bootloader
    }        // namespace detail
}    // namespace Board