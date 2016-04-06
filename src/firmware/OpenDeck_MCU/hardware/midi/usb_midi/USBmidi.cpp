/*
 * USB API for Teensy USB Development Board
 * http://www.pjrc.com/teensy/teensyduino.html
 * Copyright (c) 2008 PJRC.COM, LLC
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include "../../usb/usb_common.h"
#include "../../usb/usb_private.h"
#include "USBmidi.h"
#include "CIN.h"

void usb_midi_class::init(uint8_t channel) {

    usb_init();
    inChannel = channel;

}

void usb_midi_class::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel)   {

    send_raw(CIN_NOTE_OFF, midiMessageNoteOff | normalizeChannel(channel), normalizeData(note), normalizeData(velocity));

}

void usb_midi_class::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel)    {

    send_raw(CIN_NOTE_ON, midiMessageNoteOn | normalizeChannel(channel), normalizeData(note), normalizeData(velocity));

}

void usb_midi_class::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {

    send_raw(CIN_CONTROL_CHANGE, midiMessageControlChange | normalizeChannel(channel), normalizeData(control), normalizeData(value));

}

void usb_midi_class::sendProgramChange(uint8_t program, uint8_t channel)    {

    send_raw(CIN_PROGRAM_CHANGE, midiMessageProgramChange | normalizeChannel(channel), normalizeData(program), 0);

}

void usb_midi_class::sendChannelAftertouch(uint8_t pressure, uint8_t channel)  {

    send_raw(CIN_CHANNEL_AFTERTOUCH, midiMessageAfterTouchChannel | normalizeChannel(channel), normalizeData(pressure), 0);

}

void usb_midi_class::sendKeyAftertouch(uint8_t note, uint8_t pressure, uint8_t channel)  {

    send_raw(CIN_KEY_AFTERTOUCH, midiMessageAfterTouchPoly | normalizeChannel(channel), normalizeData(note), normalizeData(pressure));

}

void usb_midi_class::sendPitchBend(uint16_t value, uint8_t channel)   {

    send_raw(CIN_PITCH_BEND, midiMessagePitchBend | normalizeChannel(channel), normalizeData(value), normalizeData(value >> 7));

}

void usb_midi_class::sendSysEx(uint8_t length, const uint8_t *data, bool ArrayContainsBoundaries) {

    //TODO: MIDI 2.5 lib automatically adds start and stop bytes
    //done!

    if (!ArrayContainsBoundaries)   {

        //append sysex start (0xF0) and stop (0xF7) bytes to array

        bool firstByte = true;
        bool startSent = false;

        while (length > 3) {

            if (firstByte)  {

                send_raw(CIN_SYSEX_START, 0xF0, data[0], data[1]);
                firstByte = false;
                startSent = true;
                data += 2;
                length -= 2;

            }   else {

                send_raw(CIN_SYSEX_START, data[0], data[1], data[2]);
                data += 3;
                length -= 3;

            }

        }

        if (length == 3)    {

            if (startSent)  {

                send_raw(CIN_SYSEX_START, data[0], data[1], data[2]);
                send_raw(CIN_SYSEX_STOP_1BYTE, 0xF7, 0, 0);

            }   else {

                send_raw(CIN_SYSEX_START, 0xF0, data[0], data[1]);
                send_raw(CIN_SYSEX_STOP_2BYTE, data[2], 0xF7, 0);

            }

        }

        else if (length == 2) {

            if (startSent)
                send_raw(CIN_SYSEX_STOP_3BYTE, data[0], data[1], 0xF7);

            else {

                send_raw(CIN_SYSEX_START, 0xF0, data[0], data[1]);
                send_raw(CIN_SYSEX_STOP_1BYTE, 0xF7, 0, 0);

            }

        }

        else if (length == 1) {

            if (startSent)  send_raw(CIN_SYSEX_STOP_2BYTE, data[0], 0xF7, 0);
            else            send_raw(CIN_SYSEX_STOP_3BYTE, 0xF0, data[0], 0xF7);

        }

    }   else {

        while (length > 3) {

            send_raw(CIN_SYSEX_START, data[0], data[1], data[2]);
            data += 3;
            length -= 3;

        }

        if (length == 3)        send_raw(CIN_SYSEX_STOP_3BYTE, data[0], data[1], data[2]);
        else if (length == 2)   send_raw(CIN_SYSEX_STOP_2BYTE, data[0], data[1], 0);
        else if (length == 1)   send_raw(CIN_SYSEX_STOP_1BYTE, data[0], 0, 0);

    }

}

void usb_midi_class::send_raw(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)   {

    uint8_t intr_state, timeout;

    if (!usb_configuration) return;
    intr_state = SREG;
    cli();
    UENUM = MIDI_TX_ENDPOINT;
    timeout = UDFNUML + 2;

    while (1) {

        //are we ready to transmit?
        if (UEINTX & (1<<RWAL)) break;
        SREG = intr_state;
        if (UDFNUML == timeout) return;
        if (!usb_configuration) return;
        intr_state = SREG;
        cli();
        UENUM = MIDI_TX_ENDPOINT;

    }

    UEDATX = b0;
    UEDATX = b1;
    UEDATX = b2;
    UEDATX = b3;

    if (!(UEINTX & (1<<RWAL))) UEINTX = 0x3A;
    SREG = intr_state;

}

void usb_midi_class::send_now(void) {

    uint8_t intr_state;

    if (!usb_configuration) return;
    intr_state = SREG;
    cli();
    UENUM = MIDI_TX_ENDPOINT;

    if (UEBCLX != MIDI_TX_SIZE)
        UEINTX = 0x3A;

    SREG = intr_state;

}

bool usb_midi_class::read() {

    uint8_t decodedChannel, intr_state;
    uint8_t b0, b1, b2, b3, cin, messageType;

    intr_state = SREG;
    cli();

    if (!usb_configuration) {

        SREG = intr_state;
        return false;

    }

    UENUM = MIDI_RX_ENDPOINT;

    retry:
    decodedChannel = UEINTX;

    if (!(decodedChannel & (1<<RWAL))) {

        if (decodedChannel & (1<<RXOUTI)) {

            UEINTX = 0x6B;
            goto retry;

        }

        SREG = intr_state;
        return false;

    }

    b0 = UEDATX;
    b1 = UEDATX;
    b2 = UEDATX;
    b3 = UEDATX;

    if (!(UEINTX & (1<<RWAL))) UEINTX = 0x6B;
    SREG = intr_state;

    cin = b0 & 0x0F;
    messageType = b1 & 0xF0;
    decodedChannel = (b1 & 0x0F) + 1;

    if (cin >= CIN_NOTE_OFF && cin <= CIN_PITCH_BEND) {

        if (inChannel && inChannel != decodedChannel) {

            //ignore other channels when user wants single channel read
            return false;

        }

        if (cin == CIN_NOTE_OFF && messageType == midiMessageNoteOff) {

            //note off
            msg_type = midiMessageNoteOff;
            goto return_message;

        }

        if (cin == CIN_NOTE_ON && messageType == midiMessageNoteOn) {

            //note on
            msg_type = midiMessageNoteOn;
            goto return_message;

        }

        if (cin == CIN_CONTROL_CHANGE && messageType == midiMessageControlChange) {

            //control change
            msg_type = midiMessageControlChange;
            goto return_message;

        }

        if (cin == CIN_PROGRAM_CHANGE && messageType == midiMessageProgramChange) {

            //program change
            msg_type = midiMessageProgramChange;
            goto return_message;

        }

        if (cin == CIN_CHANNEL_AFTERTOUCH && messageType == midiMessageAfterTouchChannel) {

            //aftertouch
            msg_type = midiMessageAfterTouchChannel;
            goto return_message;

        }

        if (cin == CIN_PITCH_BEND && messageType == midiMessagePitchBend) {

            //pitch bend
            msg_type = midiMessagePitchBend;
            goto return_message;

        }

        return false;

        return_message:
        // only update these when returning true for a parsed message
        // all other return cases will preserve these user-visible values
        msg_channel = decodedChannel;
        msg_data1 = b2;
        msg_data2 = b3;
        return true;

    }

    if (cin == CIN_SYSEX_START) {

        read_sysex_byte(b1);
        read_sysex_byte(b2);
        read_sysex_byte(b3);
        return false;

    }

    if (cin >= CIN_SYSEX_STOP_1BYTE && cin <= CIN_SYSEX_STOP_3BYTE) {

        read_sysex_byte(b1);
        if (cin >= CIN_SYSEX_STOP_2BYTE) read_sysex_byte(b2);
        if (cin == CIN_SYSEX_STOP_3BYTE) read_sysex_byte(b3);
        msg_data1 = msg_sysex_len;
        msg_sysex_len = 0;
        msg_type = midiMessageSystemExclusive;
        return true;

    }

    if (cin == midiMessageSystemExclusive) {

        //TODO: does this need to be a full MIDI parser?
        //What software actually uses this message type in practice?
        //opendeck does!

        if (msg_sysex_len > 0) {

            //From David Sorlien, dsorlien at gmail.com, http://axe4live.wordpress.com
            //OSX sometimes uses Single Byte Unparsed to
            //send bytes in the middle of a SYSEX message.
            read_sysex_byte(b1);

        }

    }

    return false;

}

void usb_midi_class::read_sysex_byte(uint8_t b) {

    if (msg_sysex_len < MIDI_SYSEX_ARRAY_SIZE)
        msg_sysex[msg_sysex_len++] = b;

}

usb_midi_class usbMIDI;
