/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/dispatch_info.h"

#include "opencl/source/kernel/kernel.h"

namespace NEO {
bool DispatchInfo::usesSlm() const {
    return (kernel == nullptr) ? false : kernel->getSlmTotalSize(pClDevice->getRootDeviceIndex()) > 0;
}

bool DispatchInfo::usesStatelessPrintfSurface() const {
    return (kernel == nullptr) ? false : kernel->hasPrintfOutput(pClDevice->getRootDeviceIndex());
}

uint32_t DispatchInfo::getRequiredScratchSize() const {
    return (kernel == nullptr) ? 0 : kernel->getScratchSize(pClDevice->getRootDeviceIndex());
}

uint32_t DispatchInfo::getRequiredPrivateScratchSize() const {
    return (kernel == nullptr) ? 0 : kernel->getPrivateScratchSize(pClDevice->getRootDeviceIndex());
}

Kernel *MultiDispatchInfo::peekMainKernel() const {
    if (dispatchInfos.size() == 0) {
        return nullptr;
    }
    return mainKernel ? mainKernel : dispatchInfos.begin()->getKernel();
}

Kernel *MultiDispatchInfo::peekParentKernel() const {
    return (mainKernel && mainKernel->isParentKernel) ? mainKernel : nullptr;
}

void MultiDispatchInfo::backupUnifiedMemorySyncRequirement() {
    for (const auto &dispatchInfo : dispatchInfos) {
        dispatchInfo.getKernel()->setUnifiedMemorySyncRequirement(true);
    }
}
} // namespace NEO
