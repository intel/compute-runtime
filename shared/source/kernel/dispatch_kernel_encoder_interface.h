/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
class GraphicsAllocation;

struct DispatchKernelEncoderI {
  public:
    virtual bool hasBarriers() = 0;
    virtual uint32_t getSlmTotalSize() = 0;
    virtual uint32_t getBindingTableOffset() = 0;
    virtual uint32_t getBorderColor() = 0;
    virtual uint32_t getSamplerTableOffset() = 0;
    virtual uint32_t getNumSurfaceStates() = 0;
    virtual uint32_t getNumSamplers() = 0;
    virtual uint32_t getSimdSize() = 0;
    virtual uint32_t getSizeCrossThreadData() = 0;
    virtual uint32_t getPerThreadScratchSize() = 0;
    virtual uint32_t getPerThreadExecutionMask() = 0;
    virtual uint32_t getSizePerThreadData() = 0;
    virtual uint32_t getSizePerThreadDataForWholeGroup() = 0;
    virtual uint32_t getSizeSurfaceStateHeapData() = 0;
    virtual uint32_t *getCountOffsets() = 0;
    virtual uint32_t *getSizeOffsets() = 0;
    virtual uint32_t *getLocalWorkSize() = 0;
    virtual uint32_t getNumGrfRequired() = 0;
    virtual uint32_t getThreadsPerThreadGroupCount() = 0;
    virtual GraphicsAllocation *getIsaAllocation() = 0;
    virtual bool hasGroupCounts() = 0;
    virtual bool hasGroupSize() = 0;
    virtual const void *getSurfaceStateHeap() = 0;
    virtual const void *getDynamicStateHeap() = 0;
    virtual const void *getCrossThread() = 0;
    virtual const void *getPerThread() = 0;
    virtual ~DispatchKernelEncoderI() = default;

  protected:
    uint32_t groupCountOffsets[3] = {};
    uint32_t groupSizeOffsets[3] = {};
    uint32_t localWorkSize[3] = {};
};
} // namespace NEO