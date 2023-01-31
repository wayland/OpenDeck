/*

Copyright 2015-2022 Igor Petrovic

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

#include "board/Board.h"
#include "board/common/communication/USBOverSerial/USBOverSerial.h"
#include "Commands.h"
#include "core/src/Timing.h"
#include "core/src/MCU.h"
#include "protocol/midi/MIDI.h"

using namespace board;
using namespace protocol;

namespace
{
    /// Time in milliseconds after which USB connection state should be checked
    constexpr uint32_t           USB_CONN_CHECK_TIME = 2000;
    uint8_t                      uartReadBuffer[BUFFER_SIZE_USB_OVER_SERIAL];
    usbOverSerial::USBReadPacket readPacket(uartReadBuffer, BUFFER_SIZE_USB_OVER_SERIAL);
    MIDI::usbMIDIPacket_t        usbMIDIPacket;

    void checkUSBconnection()
    {
        static uint32_t lastCheckTime       = 0;
        static bool     lastConnectionState = false;

        if (core::timing::ms() - lastCheckTime > USB_CONN_CHECK_TIME)
        {
            bool newState = usb::isUSBconnected();

            if (lastConnectionState != newState)
            {
                uint8_t data[2] = {
                    static_cast<uint8_t>(usbLink::internalCMD_t::USB_STATE),
                    newState,
                };

                usbOverSerial::USBWritePacket packet(usbOverSerial::packetType_t::INTERNAL,
                                                     data,
                                                     2,
                                                     BUFFER_SIZE_USB_OVER_SERIAL);
                usbOverSerial::write(HW_UART_CHANNEL_USB_LINK, packet);

                lastConnectionState = newState;
            }

            lastCheckTime = core::timing::ms();
        }
    }

    void sendUniqueID()
    {
        core::mcu::uniqueID_t uniqueID;
        core::mcu::uniqueID(uniqueID);

        uint8_t data[11] = {
            static_cast<uint8_t>(usbLink::internalCMD_t::UNIQUE_ID),
            uniqueID[0],
            uniqueID[1],
            uniqueID[2],
            uniqueID[3],
            uniqueID[4],
            uniqueID[5],
            uniqueID[6],
            uniqueID[7],
            uniqueID[8],
            uniqueID[9],
        };

        usbOverSerial::USBWritePacket packet(usbOverSerial::packetType_t::INTERNAL,
                                             data,
                                             11,
                                             BUFFER_SIZE_USB_OVER_SERIAL);
        usbOverSerial::write(HW_UART_CHANNEL_USB_LINK, packet);
    }

    void sendLinkReady()
    {
        uint8_t data[1] = {
            static_cast<uint8_t>(usbLink::internalCMD_t::LINK_READY),
        };

        usbOverSerial::USBWritePacket packet(usbOverSerial::packetType_t::INTERNAL,
                                             data,
                                             1,
                                             BUFFER_SIZE_USB_OVER_SERIAL);
        usbOverSerial::write(HW_UART_CHANNEL_USB_LINK, packet);
    }
}    // namespace

int main()
{
    board::init();

    while (1)
    {
        // USB MIDI -> UART
        if (usb::readMIDI(usbMIDIPacket))
        {
            usbOverSerial::USBWritePacket packet(usbOverSerial::packetType_t::MIDI,
                                                 &usbMIDIPacket[0],
                                                 4,
                                                 BUFFER_SIZE_USB_OVER_SERIAL);

            if (usbOverSerial::write(HW_UART_CHANNEL_USB_LINK, packet))
            {
                board::io::indicators::indicateTraffic(board::io::indicators::source_t::USB,
                                                       board::io::indicators::direction_t::INCOMING);
            }
        }

        // UART -> USB
        if (usbOverSerial::read(HW_UART_CHANNEL_USB_LINK, readPacket))
        {
            if (readPacket.type() == usbOverSerial::packetType_t::MIDI)
            {
                for (size_t i = 0; i < sizeof(usbMIDIPacket); i++)
                {
                    usbMIDIPacket[i] = readPacket[i];
                }

                if (usb::writeMIDI(usbMIDIPacket))
                {
                    board::io::indicators::indicateTraffic(board::io::indicators::source_t::USB,
                                                           board::io::indicators::direction_t::OUTGOING);
                }
            }
            else if (readPacket.type() == usbOverSerial::packetType_t::INTERNAL)
            {
                auto cmd = static_cast<usbLink::internalCMD_t>(readPacket[0]);

                switch (cmd)
                {
                case usbLink::internalCMD_t::REBOOT_BTLDR:
                {
                    // use received data as the magic bootloader value
                    uint32_t magicVal = readPacket[1];
                    magicVal <<= 8;
                    magicVal |= readPacket[2];
                    magicVal <<= 8;
                    magicVal |= readPacket[3];
                    magicVal <<= 8;
                    magicVal |= readPacket[4];

                    bootloader::setMagicBootValue(magicVal);
                    reboot();
                }
                break;

                case usbLink::internalCMD_t::DISCONNECT_USB:
                {
                    board::usb::deInit();
                }
                break;

                case usbLink::internalCMD_t::CONNECT_USB:
                {
                    if (!usb::isUSBconnected())
                    {
                        board::usb::init();
                    }
                }
                break;

                case usbLink::internalCMD_t::UNIQUE_ID:
                {
                    sendUniqueID();
                }
                break;

                case usbLink::internalCMD_t::LINK_READY:
                {
                    sendLinkReady();
                }
                break;

                case usbLink::internalCMD_t::FACTORY_RESET:
                {
                    board::io::indicators::indicateFactoryReset();
                }
                break;

                default:
                    break;
                }
            }

            // clear out any stored information in packet since we are reusing it
            readPacket.reset();
        }

        checkUSBconnection();
    }
}