/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
class GraphicsAllocation;
struct ImplicitArgs;
struct KernelDescriptor;

enum class SlmPolicy {
    slmPolicyNone,
    slmPolicyLargeSlm,
    slmPolicyLargeData
};

struct DispatchKernelEncoderI {
    virtual ~DispatchKernelEncoderI() = default;

    virtual const KernelDescriptor &getKernelDescriptor() const = 0;
    virtual const uint32_t *getGroupSize() const = 0;
    virtual uint32_t getSlmTotalSize() const = 0;
    virtual SlmPolicy getSlmPolicy() const = 0;

    virtual const uint8_t *getCrossThreadData() const = 0;
    virtual uint32_t getCrossThreadDataSize() const = 0;

    virtual uint32_t getThreadExecutionMask() const = 0;
    virtual uint32_t getNumThreadsPerThreadGroup() const = 0;
    virtual const uint8_t *getPerThreadData() const = 0;
    virtual uint32_t getPerThreadDataSize() const = 0;
    virtual uint32_t getPerThreadDataSizeForWholeThreadGroup() const = 0;

    virtual const uint8_t *getSurfaceStateHeapData() const = 0;
    virtual uint32_t getSurfaceStateHeapDataSize() const = 0;

    virtual GraphicsAllocation *getIsaAllocation() const = 0;
    virtual uint64_t getIsaOffsetInParentAllocation() const = 0;
    virtual const uint8_t *getDynamicStateHeapData() const = 0;

    virtual uint32_t getRequiredWorkgroupOrder() const = 0;
    virtual bool requiresGenerationOfLocalIdsByRuntime() const = 0;

    virtual ImplicitArgs *getImplicitArgs() const = 0;
    virtual void patchBindlessOffsetsInCrossThreadData(uint64_t bindlessSurfaceStateBaseOffset) const = 0;
    virtual void patchSamplerBindlessOffsetsInCrossThreadData(uint64_t samplerStateOffset) const = 0;
};
} // namespace NEO
