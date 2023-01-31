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

#include "io/buttons/Buttons.h"

namespace io
{
    class ButtonsFilter : public io::Buttons::Filter
    {
        public:
        ButtonsFilter() = default;

        bool isFiltered(size_t index, uint8_t& numberOfReadings, uint32_t& states) override
        {
            return false;
        }
    };
}    // namespace io