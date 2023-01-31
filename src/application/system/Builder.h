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

#pragma once

#include "system/System.h"
#include "io/buttons/Buttons.h"
#include "io/buttons/Filter.h"
#include "io/encoders/Encoders.h"
#include "io/encoders/Filter.h"
#include "io/analog/Analog.h"
#include "io/analog/Filter.h"
#include "io/leds/LEDs.h"
#include "io/i2c/I2C.h"
#include "io/i2c/peripherals/Builder.h"
#include "io/touchscreen/Touchscreen.h"
#include "io/touchscreen/model/Builder.h"
#include "protocol/midi/MIDI.h"
#include "database/Layout.h"

namespace sys
{
    class Builder
    {
        public:
        class HWA
        {
            public:
            class IO
            {
                // these are just renames of existing HWAs in order to have them in the same namespace
                public:
                using Analog         = ::io::Analog::HWA;
                using Buttons        = ::io::Buttons::HWA;
                using CDCPassthrough = ::io::Touchscreen::CDCPassthrough;
                using Display        = ::io::I2C::Peripheral::HWA;
                using Encoders       = ::io::Encoders::HWA;
                using LEDs           = ::io::LEDs::HWA;
                using Touchscreen    = ::io::Touchscreen::HWA;

                virtual ~IO() = default;

                virtual Analog&         analog()         = 0;
                virtual Buttons&        buttons()        = 0;
                virtual CDCPassthrough& cdcPassthrough() = 0;
                virtual Display&        display()        = 0;
                virtual Encoders&       encoders()       = 0;
                virtual LEDs&           leds()           = 0;
                virtual Touchscreen&    touchscreen()    = 0;
            };

            class Protocol
            {
                public:
                class MIDI
                {
                    public:
                    using USB = ::protocol::MIDI::HWAUSB;
                    using DIN = ::protocol::MIDI::HWADIN;
                    using BLE = ::protocol::MIDI::HWABLE;

                    virtual ~MIDI() = default;

                    virtual USB& usb() = 0;
                    virtual DIN& din() = 0;
                    virtual BLE& ble() = 0;
                };

                virtual ~Protocol() = default;

                virtual MIDI& midi() = 0;
            };

            using Database = ::database::Admin::StorageAccess;
            using System   = ::sys::Instance::HWA;

            virtual ~HWA() = default;

            virtual IO&       io()       = 0;
            virtual Protocol& protocol() = 0;
            virtual System&   system()   = 0;
            virtual Database& database() = 0;
        };

        Builder(HWA& hwa)
            : _hwa(hwa)
            , _components(*this)
        {
            _io.at(static_cast<size_t>(::io::ioComponent_t::BUTTONS))     = &_buttons;
            _io.at(static_cast<size_t>(::io::ioComponent_t::ENCODERS))    = &_encoders;
            _io.at(static_cast<size_t>(::io::ioComponent_t::ANALOG))      = &_analog;
            _io.at(static_cast<size_t>(::io::ioComponent_t::LEDS))        = &_leds;
            _io.at(static_cast<size_t>(::io::ioComponent_t::I2C))         = &_i2c;
            _io.at(static_cast<size_t>(::io::ioComponent_t::TOUCHSCREEN)) = &_touchscreen;

            _protocol.at(static_cast<size_t>(::protocol::protocol_t::MIDI)) = &_midi;
        }

        sys::Instance::Components& components()
        {
            return _components;
        }

        sys::Instance::HWA& hwa()
        {
            return _hwa.system();
        }

        private:
        HWA&                                                                           _hwa;
        database::AppLayout                                                            _dbLayout;
        database::Admin                                                                _database            = database::Admin(_hwa.database(), _dbLayout, DATABASE_INIT_DATA);
        io::Buttons::Database                                                          _buttonsDatabase     = io::Buttons::Database(_database);
        io::Analog::Database                                                           _analogDatabase      = io::Analog::Database(_database);
        io::Encoders::Database                                                         _encodersDatabase    = io::Encoders::Database(_database);
        io::LEDs::Database                                                             _ledsDatabase        = io::LEDs::Database(_database);
        io::Touchscreen::Database                                                      _touchscreenDatabase = io::Touchscreen::Database(_database);
        io::I2CPeripheralBuilder::DisplayDatabase                                      _displayDatabase     = io::I2CPeripheralBuilder::DisplayDatabase(_database);
        protocol::MIDI::Database                                                       _midiDatabase        = protocol::MIDI::Database(_database);
        io::EncodersFilter                                                             _encodersFilter;
        io::ButtonsFilter                                                              _buttonsFilter;
        io::AnalogFilter                                                               _analogFilter;
        io::Analog                                                                     _analog            = io::Analog(_hwa.io().analog(), _analogFilter, _analogDatabase);
        io::Buttons                                                                    _buttons           = io::Buttons(_hwa.io().buttons(), _buttonsFilter, _buttonsDatabase);
        io::LEDs                                                                       _leds              = io::LEDs(_hwa.io().leds(), _ledsDatabase);
        io::Encoders                                                                   _encoders          = io::Encoders(_hwa.io().encoders(), _encodersFilter, _encodersDatabase, 1);
        io::Touchscreen                                                                _touchscreen       = io::Touchscreen(_hwa.io().touchscreen(), _touchscreenDatabase, _hwa.io().cdcPassthrough());
        io::TouchscreenModelBuilder                                                    _touchscreenModels = io::TouchscreenModelBuilder(_hwa.io().touchscreen());
        io::I2C                                                                        _i2c;
        io::I2CPeripheralBuilder                                                       _i2cPeripherals = io::I2CPeripheralBuilder(_hwa.io().display(), _displayDatabase);
        protocol::MIDI                                                                 _midi           = protocol::MIDI(_hwa.protocol().midi().usb(), _hwa.protocol().midi().din(), _hwa.protocol().midi().ble(), _midiDatabase);
        std::array<io::Base*, static_cast<size_t>(io::ioComponent_t::AMOUNT)>          _io             = {};
        std::array<protocol::Base*, static_cast<size_t>(protocol::protocol_t::AMOUNT)> _protocol       = {};

        class Components : public sys::Instance::Components
        {
            public:
            Components(Builder& builder)
                : _builder(builder)
            {}

            std::array<io::Base*, static_cast<size_t>(io::ioComponent_t::AMOUNT)>& io() override
            {
                return _builder._io;
            }

            std::array<protocol::Base*, static_cast<size_t>(protocol::protocol_t::AMOUNT)>& protocol() override
            {
                return _builder._protocol;
            }

            database::Admin& database() override
            {
                return _builder._database;
            }

            private:
            Builder& _builder;
        };

        Components _components;
    };
}    // namespace sys
