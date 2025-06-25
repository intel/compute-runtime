/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/command_to_patch.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel.h"
#include <level_zero/ze_api.h>

#include <memory>
#include <vector>

namespace L0::MCL {

class MutableKernelGroup {
  public:
    virtual ~MutableKernelGroup() = default;

    MutableKernelGroup(uint32_t numKernels, ze_kernel_handle_t *kernelHandles, uint32_t inlineDataSize, uint32_t maxPerThreadDataSize);

    std::vector<std::unique_ptr<MutableKernel>> &getKernelsInGroup() { return kernelsInAppend; }

    uint32_t getMaxAppendIndirectHeapSize() const { return maxAppendIndirectHeapSize; }

    uint32_t getMaxAppendScratchSize(uint32_t slotId) const { return maxAppendScratchSize[slotId]; }

    MutableKernel *getCurrentMutableKernel() { return currentMutableKernel; }

    void setCurrentMutableKernel(Kernel *kernel);

    bool isScratchNeeded() const { return (maxAppendScratchSize[0] > 0) || (maxAppendScratchSize[1] > 0); }

    uint32_t getMaxIsaSize() const { return maxIsaSize; }

    void setPrefetchCmd(const CommandToPatch &cmd) { prefetchCmd = cmd; }

    const CommandToPatch &getPrefetchCmd() const { return prefetchCmd; }

    NEO::GraphicsAllocation *getIohForPrefetch() const { return iohForPrefetch; }

    void setIohForPrefetch(NEO::GraphicsAllocation *ioh) { iohForPrefetch = ioh; }

    void setScratchAddressPatchIndex(size_t patchIndex) { scratchAddressPatchIndex = patchIndex; }

    size_t getScratchAddressPatchIndex() const { return scratchAddressPatchIndex; };

  protected:
    std::vector<std::unique_ptr<MutableKernel>> kernelsInAppend;
    CommandToPatch prefetchCmd = {};

    MutableKernel *currentMutableKernel = nullptr;
    NEO::GraphicsAllocation *iohForPrefetch = nullptr;

    size_t scratchAddressPatchIndex = undefined<size_t>;

    uint32_t maxAppendScratchSize[2] = {0, 0};
    uint32_t maxAppendIndirectHeapSize = 0;
    uint32_t maxIsaSize = 0;
};

} // namespace L0::MCL
