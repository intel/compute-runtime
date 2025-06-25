/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

#include <array>
#include <memory>

namespace L0::MCL {
class MutableIndirectData {
  public:
    struct Offsets {
        CrossThreadDataOffset localWorkSize = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset localWorkSize2 = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset enqLocalWorkSize = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset globalWorkSize = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset numWorkGroups = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset workDimensions = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset globalWorkOffset = undefined<CrossThreadDataOffset>;
    };

    MutableIndirectData(std::unique_ptr<Offsets> offsets,
                        ArrayRef<uint8_t> crossThreadData, ArrayRef<uint8_t> perThreadData,
                        ArrayRef<uint8_t> inlineData)
        : offsets(std::move(offsets)), crossThreadData(crossThreadData), perThreadData(perThreadData), inlineData(inlineData){};

    inline void setIfDefined(CrossThreadDataOffset offset, std::array<uint32_t, 3> data);
    void setLocalWorkSize(std::array<uint32_t, 3> localWorkSize);
    void setLocalWorkSize2(std::array<uint32_t, 3> localWorkSize2);
    void setEnqLocalWorkSize(std::array<uint32_t, 3> enqLocalWorkSize);
    void setGlobalWorkSize(std::array<uint32_t, 3> globalWorkSize);
    void setNumWorkGroups(std::array<uint32_t, 3> numWorkGroups);
    void setWorkDimensions(uint32_t workDimensions);
    void setGlobalWorkOffset(std::array<uint32_t, 3> globalWorkOffset);

    void setPerThreadData(ArrayRef<const uint8_t> perThreadData);
    void setCrossThreadData(ArrayRef<const uint8_t> crossThreadData);

    size_t getCrossThreadDataSize() const {
        return crossThreadData.size();
    }
    const Offsets &getIndirectDataOffsets() const {
        return *offsets;
    }

    void copyIndirectData(MutableIndirectData *destinationIndirectData);
    void setAddress(CrossThreadDataOffset offset, uint64_t address, size_t addressSize);

  protected:
    IndirectObjectHeapOffset offsetInHeap{};
    std::unique_ptr<Offsets> offsets;
    ArrayRef<uint8_t> crossThreadData;
    ArrayRef<uint8_t> perThreadData;
    ArrayRef<uint8_t> inlineData;
};
} // namespace L0::MCL
