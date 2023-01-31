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

namespace io
{
    class Buttons : public io::Base
    {
        public:
        class Collection : public common::BaseCollection<0>
        {
            public:
            Collection() = delete;
        };

        enum
        {
            GROUP_DIGITAL_INPUTS,
        };

        enum class type_t : uint8_t
        {
            MOMENTARY,    ///< Event on press and release.
            LATCHING,     ///< Event between presses only.
            AMOUNT        ///< Total number of button types.
        };

        enum class messageType_t : uint8_t
        {
            NOTE,
            PROGRAM_CHANGE,
            CONTROL_CHANGE,
            CONTROL_CHANGE_RESET,
            MMC_STOP,
            MMC_PLAY,
            MMC_RECORD,
            MMC_PAUSE,
            REAL_TIME_CLOCK,
            REAL_TIME_START,
            REAL_TIME_CONTINUE,
            REAL_TIME_STOP,
            REAL_TIME_ACTIVE_SENSING,
            REAL_TIME_SYSTEM_RESET,
            PROGRAM_CHANGE_INC,
            PROGRAM_CHANGE_DEC,
            NONE,
            PRESET_CHANGE,
            MULTI_VAL_INC_RESET_NOTE,
            MULTI_VAL_INC_DEC_NOTE,
            MULTI_VAL_INC_RESET_CC,
            MULTI_VAL_INC_DEC_CC,
            NOTE_OFF_ONLY,
            CONTROL_CHANGE0_ONLY,
            RESERVED,
            PROGRAM_CHANGE_OFFSET_INC,
            PROGRAM_CHANGE_OFFSET_DEC,
            AMOUNT
        };

        class HWA
        {
            public:
            virtual ~HWA() = default;

            virtual bool   state(size_t index, uint8_t& numberOfReadings, uint32_t& states) = 0;
            virtual size_t buttonToEncoderIndex(size_t index)                               = 0;
        };

        class Filter
        {
            public:
            virtual ~Filter() = default;

            virtual bool isFiltered(size_t index, uint8_t& numberOfReadings, uint32_t& states) = 0;
        };

        using Database = database::User<database::Config::Section::button_t,
                                        database::Config::Section::encoder_t>;

        Buttons(HWA&      hwa,
                Filter&   filter,
                Database& database)
        {}

        bool init() override
        {
            return false;
        }

        void updateSingle(size_t index, bool forceRefresh = false) override
        {
        }

        void updateAll(bool forceRefresh = false) override
        {
        }

        size_t maxComponentUpdateIndex() override
        {
            return 0;
        }

        void reset(size_t index)
        {
        }
    };
}    // namespace io