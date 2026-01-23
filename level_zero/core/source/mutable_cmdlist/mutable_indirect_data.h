/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"

#include <memory>

namespace L0::MCL {
class MutableIndirectData {
  public:
    struct Offsets {
        CrossThreadDataOffset localWorkSize[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
        CrossThreadDataOffset localWorkSize2[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
        CrossThreadDataOffset enqLocalWorkSize[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
        CrossThreadDataOffset globalWorkSize[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
        CrossThreadDataOffset numWorkGroups[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
        CrossThreadDataOffset workDimensions = undefined<CrossThreadDataOffset>;
        CrossThreadDataOffset globalWorkOffset[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
    };

    MutableIndirectData(std::unique_ptr<Offsets> offsets,
                        ArrayRef<uint8_t> crossThreadData, ArrayRef<uint8_t> perThreadData,
                        ArrayRef<uint8_t> inlineData)
        : offsets(std::move(offsets)), crossThreadData(crossThreadData), perThreadData(perThreadData), inlineData(inlineData){};

    inline void setIfDefined(const CrossThreadDataOffset (&offsets)[3], MaxChannelsArray data);
    void setLocalWorkSize(MaxChannelsArray localWorkSize);
    void setLocalWorkSize2(MaxChannelsArray localWorkSize2);
    void setEnqLocalWorkSize(MaxChannelsArray enqLocalWorkSize);
    void setGlobalWorkSize(MaxChannelsArray globalWorkSize);
    void setNumWorkGroups(MaxChannelsArray numWorkGroups);
    void setWorkDimensions(uint32_t workDimensions);
    void setGlobalWorkOffset(MaxChannelsArray globalWorkOffset);

    void setPerThreadData(ArrayRef<const uint8_t> perThreadData);
    void setCrossThreadData(ArrayRef<const uint8_t> crossThreadData);

    size_t getCrossThreadDataSize() const {
        return crossThreadData.size();
    }
    const Offsets &getIndirectDataOffsets() const {
        return *offsets;
    }
    void copyIndirectData(MutableIndirectData *destinationIndirectData) {
        destinationIndirectData->setCrossThreadData(this->crossThreadData);
    }

    void setAddress(CrossThreadDataOffset offset, uint64_t address, size_t addressSize);

  protected:
    IndirectObjectHeapOffset offsetInHeap{};
    std::unique_ptr<Offsets> offsets;
    ArrayRef<uint8_t> crossThreadData;
    ArrayRef<uint8_t> perThreadData;
    ArrayRef<uint8_t> inlineData;
};
} // namespace L0::MCL
