/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/kernel/dispatch_kernel_encoder_interface.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include "gmock/gmock.h"

#include <stdint.h>

namespace NEO {
class GraphicsAllocation;

struct MockDispatchKernelEncoder : public DispatchKernelEncoderI {
  public:
    MockDispatchKernelEncoder();
    MOCK_METHOD0(hasBarriers, bool());
    MOCK_METHOD0(getSlmTotalSize, uint32_t());
    MOCK_METHOD0(getBindingTableOffset, uint32_t());
    MOCK_METHOD0(getBorderColor, uint32_t());
    MOCK_METHOD0(getSamplerTableOffset, uint32_t());
    MOCK_METHOD0(getNumSurfaceStates, uint32_t());
    MOCK_METHOD0(getNumSamplers, uint32_t());
    MOCK_METHOD0(getSimdSize, uint32_t());
    MOCK_METHOD0(getSizeCrossThreadData, uint32_t());
    MOCK_METHOD0(getPerThreadScratchSize, uint32_t());
    MOCK_METHOD0(getPerThreadExecutionMask, uint32_t());
    MOCK_METHOD0(getSizePerThreadData, uint32_t());
    MOCK_METHOD0(getSizePerThreadDataForWholeGroup, uint32_t());
    MOCK_METHOD0(getSizeSurfaceStateHeapData, uint32_t());
    MOCK_METHOD0(getCountOffsets, uint32_t *());
    MOCK_METHOD0(getSizeOffsets, uint32_t *());
    MOCK_METHOD0(getLocalWorkSize, uint32_t *());
    MOCK_METHOD0(getNumGrfRequired, uint32_t());
    MOCK_METHOD0(getThreadsPerThreadGroupCount, uint32_t());
    MOCK_METHOD0(getIsaAllocation, GraphicsAllocation *());
    MOCK_METHOD0(hasGroupCounts, bool());
    MOCK_METHOD0(hasGroupSize, bool());
    MOCK_METHOD0(getSurfaceStateHeap, const void *());
    MOCK_METHOD0(getDynamicStateHeap, const void *());
    MOCK_METHOD0(getCrossThread, const void *());
    MOCK_METHOD0(getPerThread, const void *());

    void expectAnyMockFunctionCall();

    ::testing::NiceMock<MockGraphicsAllocation> mockAllocation;
    static constexpr uint32_t crossThreadSize = 0x40;
    static constexpr uint32_t perThreadSize = 0x20;
    uint8_t dataCrossThread[crossThreadSize];
    uint8_t dataPerThread[perThreadSize];
};
} // namespace NEO