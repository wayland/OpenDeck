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

#ifdef HW_SUPPORT_USB
#ifdef FW_BOOT

#include <avr/pgmspace.h>
#include "LUFA/Drivers/USB/USB.h"
#include "board/common/communication/usb/descriptors/common/Common.h"
#include "board/common/communication/usb/descriptors/midi/Descriptors.h"
#include "board/Board.h"
#include "board/Internal.h"
#include "core/src/Timing.h"

namespace
{
    constexpr uint32_t                     USB_TX_TIMEOUT_MS = 2000;
    USB_ClassInfo_MIDI_Device_t            _midiInterface;
    volatile board::detail::usb::txState_t _txStateMIDI;
}    // namespace

extern "C" void EVENT_USB_Device_ConfigurationChanged(void)
{
    Endpoint_ConfigureEndpoint(USB_ENDPOINT_ADDR_MIDI_IN, core::mcu::usb::ENDPOINT_TYPE_BULK, USB_ENDPOINT_SIZE_MIDI_IN_OUT, 1);
    Endpoint_ConfigureEndpoint(USB_ENDPOINT_ADDR_MIDI_OUT, core::mcu::usb::ENDPOINT_TYPE_BULK, USB_ENDPOINT_SIZE_MIDI_IN_OUT, 1);
}

extern "C" uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue, const uint16_t wIndex, const void** const descriptorAddress)
{
    const uint8_t DESCRIPTOR_TYPE   = (wValue >> 8);
    const uint8_t DESCRIPTOR_NUMBER = (wValue & 0xFF);

    const void* address = NULL;
    uint16_t    size    = 0;

    switch (DESCRIPTOR_TYPE)
    {
    case core::mcu::usb::DESC_TYPE_DEVICE:
    {
        address = board::detail::usb::deviceDescriptor(&size);
    }
    break;

    case core::mcu::usb::DESC_TYPE_CONFIGURATION:
    {
        address = board::detail::usb::cfgDescriptor(&size);
    }
    break;

    case core::mcu::usb::DESC_TYPE_STRING:
    {
        switch (DESCRIPTOR_NUMBER)
        {
        case USB_STRING_ID_LANGUAGE:
        {
            address = board::detail::usb::languageString(&size);
        }
        break;

        case USB_STRING_ID_MANUFACTURER:
        {
            address = board::detail::usb::manufacturerString(&size);
        }
        break;

        case USB_STRING_ID_PRODUCT:
        {
            address = board::detail::usb::productString(&size);
        }
        break;

        case USB_STRING_ID_UID:
        {
            // handled internally by LUFA
        }
        break;

        default:
            break;
        }
    }
    break;

    default:
        break;
    }

    *descriptorAddress = address;
    return size;
}

namespace board
{
    namespace detail::usb
    {
        void init()
        {
            _midiInterface.Config.StreamingInterfaceNumber = USB_INTERFACE_ID_AUDIO_STREAM;
            _midiInterface.Config.DataINEndpoint.Address   = USB_ENDPOINT_ADDR_MIDI_IN;
            _midiInterface.Config.DataINEndpoint.Size      = USB_ENDPOINT_SIZE_MIDI_IN_OUT;
            _midiInterface.Config.DataINEndpoint.Banks     = 1;
            _midiInterface.Config.DataOUTEndpoint.Address  = USB_ENDPOINT_ADDR_MIDI_OUT;
            _midiInterface.Config.DataOUTEndpoint.Size     = USB_ENDPOINT_SIZE_MIDI_IN_OUT;
            _midiInterface.Config.DataOUTEndpoint.Banks    = 1;

            USB_Init();
        }

        void deInit()
        {
            USB_Disable();
        }
    }    // namespace detail::usb

    namespace usb
    {
        bool isUSBconnected()
        {
            return USB_DeviceState == DEVICE_STATE_Configured;
        }

        bool readMIDI(midiPacket_t& packet)
        {
            // device must be connected and configured for the task to run
            if (USB_DeviceState != DEVICE_STATE_Configured)
            {
                return false;
            }

            // select the MIDI OUT stream
            Endpoint_SelectEndpoint(USB_ENDPOINT_ADDR_MIDI_OUT);

            // check if a MIDI command has been received
            if (Endpoint_IsOUTReceived())
            {
                // read the MIDI event packet from the endpoint
                Endpoint_Read_Stream_LE(&packet[0], sizeof(packet), NULL);

                // if the endpoint is now empty, clear the bank
                if (!(Endpoint_BytesInEndpoint()))
                {
                    Endpoint_ClearOUT();    // clear the endpoint ready for new packet
                }

                return true;
            }

            return false;
        }

        bool writeMIDI(midiPacket_t& packet)
        {
            static uint32_t timeout = 0;

            if (!isUSBconnected())
            {
                return false;
            }

            // once the transfer fails, wait USB_TX_TIMEOUT_MS ms before trying again
            if (_txStateMIDI != board::detail::usb::txState_t::DONE)
            {
                if ((core::timing::ms() - timeout) < USB_TX_TIMEOUT_MS)
                {
                    return false;
                }

                _txStateMIDI = board::detail::usb::txState_t::DONE;
                timeout      = 0;
            }

            Endpoint_SelectEndpoint(_midiInterface.Config.DataINEndpoint.Address);

            uint8_t ErrorCode;

            _txStateMIDI = board::detail::usb::txState_t::SENDING;

            if ((ErrorCode = Endpoint_Write_Stream_LE(&packet[0], sizeof(packet), NULL)) != ENDPOINT_RWSTREAM_NoError)
            {
                _txStateMIDI = board::detail::usb::txState_t::WAITING;
                return false;
            }

            if (!(Endpoint_IsReadWriteAllowed()))
            {
                Endpoint_ClearIN();
            }

            MIDI_Device_Flush(&_midiInterface);

            _txStateMIDI = board::detail::usb::txState_t::DONE;
            return true;
        }
    }    // namespace usb
}    // namespace board

#endif
#endif