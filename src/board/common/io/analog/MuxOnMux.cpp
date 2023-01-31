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

#ifdef HW_SUPPORT_ADC
#ifdef HW_DRIVER_ANALOG_INPUT_MUXONMUX

#include "core/src/util/Util.h"
#include "board/Board.h"
#include "board/Internal.h"
#include <Target.h>

using namespace board::io::analog;
using namespace board::detail;
using namespace board::detail::io::analog;

static_assert(HW_ADC_SAMPLES > 0, "At least 1 ADC sample required");

namespace
{
    constexpr size_t ANALOG_IN_BUFFER_SIZE = HW_MAX_NR_OF_ANALOG_INPUTS;

    uint8_t           _analogIndex;
    volatile uint16_t _analogBuffer[ANALOG_IN_BUFFER_SIZE];
    uint8_t           _activeMux;
    uint8_t           _activeMuxInput;
    volatile uint16_t _sample;
    volatile uint8_t  _sampleCounter;

    /// Configures one of 16 inputs/outputs on 4067 multiplexer.
    inline void setMuxInput()
    {
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_NODE_S0, PIN_INDEX_MUX_NODE_S0, core::util::BIT_READ(_activeMuxInput, 0));
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_NODE_S1, PIN_INDEX_MUX_NODE_S1, core::util::BIT_READ(_activeMuxInput, 1));
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_NODE_S2, PIN_INDEX_MUX_NODE_S2, core::util::BIT_READ(_activeMuxInput, 2));
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_NODE_S3, PIN_INDEX_MUX_NODE_S3, core::util::BIT_READ(_activeMuxInput, 3));
    }

    inline void setMux()
    {
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_CTRL_S0, PIN_INDEX_MUX_CTRL_S0, core::util::BIT_READ(_activeMux, 0));
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_CTRL_S1, PIN_INDEX_MUX_CTRL_S1, core::util::BIT_READ(_activeMux, 1));
#ifdef PIN_PORT_MUX_NODE_S2
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_NODE_S2, PIN_INDEX_MUX_NODE_S2, core::util::BIT_READ(_activeMux, 2));
#endif
#ifdef PIN_PORT_MUX_NODE_S3
        CORE_MCU_IO_SET_STATE(PIN_PORT_MUX_NODE_S3, PIN_INDEX_MUX_NODE_S3, core::util::BIT_READ(_activeMux, 3));
#endif
    }
}    // namespace

namespace board::detail::io::analog
{
    void init()
    {
        core::mcu::adc::conf_t adcConfiguration;

        adcConfiguration.prescaler = HW_ADC_PRESCALER;
        adcConfiguration.voltage   = HW_ADC_INPUT_VOLTAGE;

#ifdef ADC_EXT_REF
        adcConfiguration.externalRef = true;
#else
        adcConfiguration.externalRef = false;
#endif

        core::mcu::adc::init(adcConfiguration);

        auto pin = map::ADC_PIN(0);
        core::mcu::adc::initPin(pin);

        CORE_MCU_IO_INIT(PIN_PORT_MUX_CTRL_S0,
                         PIN_INDEX_MUX_CTRL_S0,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);

        CORE_MCU_IO_INIT(PIN_PORT_MUX_CTRL_S1,
                         PIN_INDEX_MUX_CTRL_S1,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);

#ifdef PIN_PORT_MUX_CTRL_S2
        CORE_MCU_IO_INIT(PIN_PORT_MUX_CTRL_S2,
                         PIN_INDEX_MUX_CTRL_S2,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);
#endif

#ifdef PIN_PORT_MUX_CTRL_S3
        CORE_MCU_IO_INIT(PIN_PORT_MUX_CTRL_S3,
                         PIN_INDEX_MUX_CTRL_S3,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);
#endif

        CORE_MCU_IO_INIT(PIN_PORT_MUX_NODE_S0,
                         PIN_INDEX_MUX_NODE_S0,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);

        CORE_MCU_IO_INIT(PIN_PORT_MUX_NODE_S1,
                         PIN_INDEX_MUX_NODE_S1,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);

        CORE_MCU_IO_INIT(PIN_PORT_MUX_NODE_S2,
                         PIN_INDEX_MUX_NODE_S2,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);

        CORE_MCU_IO_INIT(PIN_PORT_MUX_NODE_S3,
                         PIN_INDEX_MUX_NODE_S3,
                         core::mcu::io::pinMode_t::OUTPUT_PP,
                         core::mcu::io::pullMode_t::NONE);

        for (uint8_t i = 0; i < 3; i++)
        {
            // few dummy reads to init ADC
            core::mcu::adc::read(map::ADC_PIN(0));
        }

        core::mcu::adc::setActivePin(map::ADC_PIN(0));
        core::mcu::adc::enableIt(board::detail::io::analog::ISR_PRIORITY);
        core::mcu::adc::startItConversion();
    }

    void isr(uint16_t adcValue)
    {
        if (adcValue <= CORE_MCU_ADC_MAX_VALUE)
        {
            // always ignore first sample
            if (_sampleCounter)
            {
                _sample += adcValue;
            }

            if (++_sampleCounter == (HW_ADC_SAMPLES + 1))
            {
                _sample /= HW_ADC_SAMPLES;
                _analogBuffer[_analogIndex] = _sample;
                _analogBuffer[_analogIndex] |= ADC_NEW_READING_FLAG;
                _sample        = 0;
                _sampleCounter = 0;
                _analogIndex++;
                _activeMuxInput++;

                bool switchMux = (_activeMuxInput == HW_NR_OF_MUX_INPUTS);

                if (switchMux)
                {
                    _activeMuxInput = 0;
                    _activeMux++;

                    if (_activeMux == HW_NR_OF_MUX)
                    {
                        _activeMux   = 0;
                        _analogIndex = 0;
                    }

                    // switch to next mux once all mux inputs are read
                    setMux();
                }

                // always switch to next read pin
                setMuxInput();
            }
        }

        core::mcu::adc::startItConversion();
    }
}    // namespace board::detail::io::analog

#include "Common.cpp.include"

#endif
#endif