/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_group.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/core/source/kernel/kernel.h"

#include <algorithm>
namespace L0::MCL {

MutableKernelGroup::MutableKernelGroup(uint32_t numKernels, ze_kernel_handle_t *kernelHandles, uint32_t inlineDataSize, uint32_t maxPerThreadDataSize) {
    for (uint32_t i = 0; i < numKernels; i++) {
        auto mutableKernel = std::make_unique<MutableKernel>(kernelHandles[i], inlineDataSize, maxPerThreadDataSize);

        for (uint32_t slotId = 0; slotId < 2; slotId++) {
            maxAppendScratchSize[slotId] = std::max(maxAppendScratchSize[slotId], mutableKernel->getKernelScratchSize(slotId));
        }
        maxAppendIndirectHeapSize = std::max(maxAppendIndirectHeapSize, mutableKernel->getKernel()->getIndirectSize());
        maxIsaSize = std::max(maxIsaSize, mutableKernel->getKernel()->getImmutableData()->getIsaSize());

        this->kernelsInAppend.emplace_back(std::move(mutableKernel));
    }
}

void MutableKernelGroup::setCurrentMutableKernel(Kernel *kernel) {
    for (auto &mutableKernelPtr : this->kernelsInAppend) {
        if (kernel == mutableKernelPtr->getKernel()) {
            currentMutableKernel = mutableKernelPtr.get();
            return;
        }
    }
}

} // namespace L0::MCL
