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

#include "io/encoders/Encoders.h"

namespace io
{
    class EncodersFilter : public io::Encoders::Filter
    {
        public:
        EncodersFilter() = default;

        bool isFiltered(size_t                    index,
                        io::Encoders::position_t  position,
                        io::Encoders::position_t& filteredPosition,
                        uint32_t                  sampleTakenTime) override
        {
            return false;
        }

        void reset(size_t index) override
        {
        }

        uint32_t lastMovementTime(size_t index) override
        {
            return 0;
        }
    };
}    // namespace io