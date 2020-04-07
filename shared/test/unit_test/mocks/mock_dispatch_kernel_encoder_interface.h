/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

#include "gmock/gmock.h"

#include <stdint.h>

namespace NEO {
class GraphicsAllocation;

struct MockDispatchKernelEncoder : public DispatchKernelEncoderI {
  public:
    MockDispatchKernelEncoder();
    MOCK_CONST_METHOD0(getKernelDescriptor, const KernelDescriptor &());
    MOCK_CONST_METHOD0(getGroupSize, const uint32_t *());
    MOCK_CONST_METHOD0(getSlmTotalSize, uint32_t());

    MOCK_CONST_METHOD0(getCrossThreadData, const uint8_t *());
    MOCK_CONST_METHOD0(getCrossThreadDataSize, uint32_t());

    MOCK_CONST_METHOD0(getThreadExecutionMask, uint32_t());
    MOCK_CONST_METHOD0(getNumThreadsPerThreadGroup, uint32_t());
    MOCK_CONST_METHOD0(getPerThreadData, const uint8_t *());
    MOCK_CONST_METHOD0(getPerThreadDataSize, uint32_t());
    MOCK_CONST_METHOD0(getPerThreadDataSizeForWholeThreadGroup, uint32_t());

    MOCK_CONST_METHOD0(getSurfaceStateHeapData, const uint8_t *());
    MOCK_CONST_METHOD0(getSurfaceStateHeapDataSize, uint32_t());

    MOCK_CONST_METHOD0(getIsaAllocation, GraphicsAllocation *());
    MOCK_CONST_METHOD0(getDynamicStateHeapData, const uint8_t *());

    void expectAnyMockFunctionCall();

    ::testing::NiceMock<MockGraphicsAllocation> mockAllocation;
    static constexpr uint32_t crossThreadSize = 0x40;
    static constexpr uint32_t perThreadSize = 0x20;
    uint8_t dataCrossThread[crossThreadSize];
    uint8_t dataPerThread[perThreadSize];
    KernelDescriptor kernelDescriptor;
};
} // namespace NEO