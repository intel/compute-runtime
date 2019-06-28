/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/dispatch_info.h"

#include "runtime/kernel/kernel.h"

namespace NEO {
bool DispatchInfo::usesSlm() const {
    return (kernel == nullptr) ? false : kernel->slmTotalSize > 0;
}

bool DispatchInfo::usesStatelessPrintfSurface() const {
    return (kernel == nullptr) ? false : (kernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface != nullptr);
}

uint32_t DispatchInfo::getRequiredScratchSize() const {
    return (kernel == nullptr) ? 0 : kernel->getScratchSize();
}

uint32_t DispatchInfo::getRequiredPrivateScratchSize() const {
    return (kernel == nullptr) ? 0 : kernel->getPrivateScratchSize();
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
} // namespace NEO
