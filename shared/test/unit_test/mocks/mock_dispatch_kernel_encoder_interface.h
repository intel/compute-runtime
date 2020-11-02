/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/unit_test/mocks/mock_graphics_allocation.h"

#include "gmock/gmock.h"

#include <stdint.h>

namespace NEO {
class GraphicsAllocation;

struct MockDispatchKernelEncoder : public DispatchKernelEncoderI {
  public:
    MockDispatchKernelEncoder();

    MOCK_METHOD(const KernelDescriptor &, getKernelDescriptor, (), (const, override));

    MOCK_METHOD(const uint32_t *, getGroupSize, (), (const, override));
    MOCK_METHOD(uint32_t, getSlmTotalSize, (), (const, override));

    MOCK_METHOD(const uint8_t *, getCrossThreadData, (), (const, override));
    MOCK_METHOD(uint32_t, getCrossThreadDataSize, (), (const, override));

    MOCK_METHOD(uint32_t, getThreadExecutionMask, (), (const, override));

    MOCK_METHOD(const uint8_t *, getPerThreadData, (), (const, override));
    MOCK_METHOD(uint32_t, getPerThreadDataSize, (), (const, override));

    MOCK_METHOD(uint32_t, getPerThreadDataSizeForWholeThreadGroup, (), (const, override));

    MOCK_METHOD(const uint8_t *, getSurfaceStateHeapData, (), (const, override));
    MOCK_METHOD(uint32_t, getSurfaceStateHeapDataSize, (), (const, override));

    MOCK_METHOD(GraphicsAllocation *, getIsaAllocation, (), (const, override));
    MOCK_METHOD(const uint8_t *, getDynamicStateHeapData, (), (const, override));

    MOCK_METHOD(bool, requiresGenerationOfLocalIdsByRuntime, (), (const, override));

    uint32_t getRequiredWorkgroupOrder() const override {
        return requiredWalkGroupOrder;
    }
    uint32_t getNumThreadsPerThreadGroup() const override {
        return 1;
    }

    void expectAnyMockFunctionCall();

    ::testing::NiceMock<MockGraphicsAllocation> mockAllocation;
    static constexpr uint32_t crossThreadSize = 0x40;
    static constexpr uint32_t perThreadSize = 0x20;
    uint8_t dataCrossThread[crossThreadSize];
    uint8_t dataPerThread[perThreadSize];
    KernelDescriptor kernelDescriptor;

    uint32_t groupSizes[3];
    bool localIdGenerationByRuntime = true;
    uint32_t requiredWalkGroupOrder = 0x0u;
};
} // namespace NEO
