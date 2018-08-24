/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cstdint>
#include <array>

namespace OCLRT {
class TimestampPacket {
  public:
    enum class DataIndex : uint32_t {
        ContextStart,
        GlobalStart,
        ContextEnd,
        GlobalEnd,
        Max
    };

    enum class WriteOperationType : uint32_t {
        Start,
        End
    };

    const uint32_t *pickDataPtr() const { return &(data[0]); }

    uint64_t pickAddressForPipeControlWrite(WriteOperationType operationType) const {
        auto index = WriteOperationType::Start == operationType
                         ? static_cast<uint32_t>(DataIndex::ContextStart)
                         : static_cast<uint32_t>(DataIndex::ContextEnd);

        return reinterpret_cast<uint64_t>(&data[index]);
    }

    uint32_t pickDataValue(DataIndex index) const { return data[static_cast<uint32_t>(index)]; }

  protected:
    std::array<uint32_t, static_cast<uint32_t>(DataIndex::Max)> data = {{1, 1, 1, 1}};
};
} // namespace OCLRT
