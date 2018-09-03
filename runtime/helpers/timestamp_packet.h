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
        Submit,
        Max
    };

    enum class WriteOperationType : uint32_t {
        BeforeWalker,
        AfterWalker
    };

    bool canBeReleased() const {
        return data[static_cast<uint32_t>(DataIndex::ContextEnd)] != 1 &&
               data[static_cast<uint32_t>(DataIndex::GlobalEnd)] != 1;
    }

    uint64_t pickAddressForDataWrite(DataIndex operationType) const {
        auto index = static_cast<uint32_t>(operationType);
        return reinterpret_cast<uint64_t>(&data[index]);
    }

    void initialize() { data = {{1, 1, 1, 1, 1}}; }

  protected:
    std::array<uint32_t, static_cast<uint32_t>(DataIndex::Max)> data = {{1, 1, 1, 1, 1}};
};
} // namespace OCLRT
