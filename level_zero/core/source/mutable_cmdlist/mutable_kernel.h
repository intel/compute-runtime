/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/residency_container.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_indirect_data.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_variable_descriptor.h"
#include <level_zero/ze_api.h>

#include <vector>

namespace L0 {
struct Kernel;
} // namespace L0

namespace L0::MCL {
struct KernelDispatch;
struct MutableComputeWalker;
struct Variable;

class MutableKernel {
  public:
    virtual ~MutableKernel() = default;

    MutableKernel(ze_kernel_handle_t kernelHandle, uint32_t inlineDataSize, uint32_t maxPerThreadDataSize);

    MutationVariables &getKernelVariables() {
        return kernelVariables;
    }

    void setComputeWalker(MutableComputeWalker *computeWalker) {
        this->computeWalker = computeWalker;
    }

    MutableComputeWalker *getMutableComputeWalker() {
        return computeWalker;
    }

    void setKernelDispatch(KernelDispatch *kernelDispatch) {
        this->kernelDispatch = kernelDispatch;
    }

    KernelDispatch *getKernelDispatch() {
        return this->kernelDispatch;
    }

    uint32_t getKernelScratchSize(uint32_t slotId) const;

    L0::Kernel *getKernel() const {
        return kernel;
    }

    void allocateHostViewIndirectHeap();
    void createHostViewIndirectData(bool copyInlineData);

    void *getHostViewIndirectHeap() {
        return hostViewIndirectHeap.get();
    }

    MutableIndirectData *getHostViewIndirectData() {
        return hostViewIndirectData.get();
    }

    void copyTemplateViewIndirectData();
    void makeKernelResidencySnapshotContainer(bool saveSyncAndRegionAllocsFromInternalResidency);
    NEO::ResidencyContainer &getKernelResidencySnapshotContainer() {
        return kernelResidencySnapshotContainer;
    }

    void saveResidencyAllocationIndices(size_t syncBuffer, size_t regionBarrier) {
        syncBufferSnapshotResidencyIndex = syncBuffer;
        regionBarrierSnapshotResidencyIndex = regionBarrier;
    }

    void updateResidencySnapshotContainer();

    bool checkKernelCompatible();

  protected:
    MutationVariables kernelVariables;
    NEO::ResidencyContainer kernelResidencySnapshotContainer;

    std::unique_ptr<MutableIndirectData> hostViewIndirectData;
    std::unique_ptr<uint8_t[]> hostViewIndirectHeap;

    MutableComputeWalker *computeWalker = nullptr;
    KernelDispatch *kernelDispatch = nullptr;
    L0::Kernel *kernel = nullptr;
    size_t syncBufferSnapshotResidencyIndex = std::numeric_limits<size_t>::max();
    size_t regionBarrierSnapshotResidencyIndex = std::numeric_limits<size_t>::max();

    uint32_t inlineDataSize = 0;
    uint32_t maxPerThreadDataSize = 0;
};

} // namespace L0::MCL
