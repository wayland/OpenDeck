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
#include "board/Internal.h"
#include "core/src/util/Util.h"

namespace
{
    // delay for 150ns
    constexpr uint32_t LOOPS = 150 / (((1.0 / CORE_MCU_CPU_FREQ_MHZ * 1000000)) * 1000000000);
}    // namespace

namespace board::detail::io
{
    void spiWait()
    {
        for (uint32_t i = 0; i < LOOPS; i++)
        {
            CORE_MCU_NOP();
        }
    }
}    // namespace board::detail::io