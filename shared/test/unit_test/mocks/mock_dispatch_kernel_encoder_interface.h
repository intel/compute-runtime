/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <stdint.h>

namespace NEO {
class GraphicsAllocation;

struct MockDispatchKernelEncoder : public DispatchKernelEncoderI {
  public:
    MockDispatchKernelEncoder() = default;

    uint32_t getRequiredWorkgroupOrder() const override {
        return requiredWalkGroupOrder;
    }
    uint32_t getNumThreadsPerThreadGroup() const override {
        return numThreadsPerThreadGroup;
    }

    NEO::ImplicitArgs *getImplicitArgs() const override { return nullptr; }

    void patchBindlessOffsetsInCrossThreadData(uint64_t bindlessSurfaceStateBaseOffset) const override { return; };
    void patchSamplerBindlessOffsetsInCrossThreadData(uint64_t samplerStateOffset) const override {
        samplerStateOffsetPassed = samplerStateOffset;
    }

    MockGraphicsAllocation mockAllocation{};
    static constexpr uint32_t crossThreadSize = 0x40;
    static constexpr uint32_t perThreadSize = 0x20;
    uint8_t dataCrossThread[crossThreadSize]{};
    uint8_t dataPerThread[perThreadSize]{};
    uint32_t groupSizes[3]{32, 1, 1};
    uint32_t requiredWalkGroupOrder = 0x0u;
    KernelDescriptor kernelDescriptor{};
    uint32_t numThreadsPerThreadGroup = 1;

    mutable uint64_t samplerStateOffsetPassed = 0u;

    ADDMETHOD_CONST_NOBASE(getKernelDescriptor, const KernelDescriptor &, kernelDescriptor, ());
    ADDMETHOD_CONST_NOBASE(getGroupSize, const uint32_t *, groupSizes, ());
    ADDMETHOD_CONST_NOBASE(getSlmTotalSize, uint32_t, 0u, ());
    ADDMETHOD_CONST_NOBASE(getCrossThreadData, const uint8_t *, dataCrossThread, ());
    ADDMETHOD_CONST_NOBASE(getCrossThreadDataSize, uint32_t, crossThreadSize, ());
    ADDMETHOD_CONST_NOBASE(getThreadExecutionMask, uint32_t, 0u, ());
    ADDMETHOD_CONST_NOBASE(getPerThreadData, const uint8_t *, dataPerThread, ());
    ADDMETHOD_CONST_NOBASE(getPerThreadDataSize, uint32_t, perThreadSize, ());
    ADDMETHOD_CONST_NOBASE(getPerThreadDataSizeForWholeThreadGroup, uint32_t, 0u, ());
    ADDMETHOD_CONST_NOBASE(getSurfaceStateHeapData, const uint8_t *, nullptr, ());
    ADDMETHOD_CONST_NOBASE(getSurfaceStateHeapDataSize, uint32_t, 0u, ());
    ADDMETHOD_CONST_NOBASE(getIsaAllocation, GraphicsAllocation *, &mockAllocation, ());
    ADDMETHOD_CONST_NOBASE(getDynamicStateHeapData, const uint8_t *, nullptr, ());
    ADDMETHOD_CONST_NOBASE(requiresGenerationOfLocalIdsByRuntime, bool, true, ());
    ADDMETHOD_CONST_NOBASE(getSlmPolicy, SlmPolicy, SlmPolicy::slmPolicyNone, ());
    ADDMETHOD_CONST_NOBASE(getIsaOffsetInParentAllocation, uint64_t, 0lu, ());
};
} // namespace NEO
