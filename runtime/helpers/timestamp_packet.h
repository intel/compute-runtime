/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
