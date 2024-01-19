/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/dispatch_info.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {

bool DispatchInfo::usesSlm() const {
    return (kernel == nullptr) ? false : kernel->getSlmTotalSize() > 0;
}

bool DispatchInfo::usesStatelessPrintfSurface() const {
    return (kernel == nullptr) ? false : kernel->hasPrintfOutput();
}

uint32_t DispatchInfo::getRequiredScratchSize(uint32_t slotId) const {
    return (kernel == nullptr) ? 0 : kernel->getScratchSize(slotId);
}

MultiDispatchInfo::~MultiDispatchInfo() {
    for (MemObj *redescribedSurface : redescribedSurfaces) {
        redescribedSurface->release();
    }
}

void MultiDispatchInfo::pushRedescribedMemObj(std::unique_ptr<MemObj> memObj) {
    redescribedSurfaces.push_back(memObj.release());
}

Kernel *MultiDispatchInfo::peekMainKernel() const {
    if (dispatchInfos.size() == 0) {
        return nullptr;
    }
    return mainKernel ? mainKernel : dispatchInfos.begin()->getKernel();
}

void MultiDispatchInfo::backupUnifiedMemorySyncRequirement() {
    for (const auto &dispatchInfo : dispatchInfos) {
        dispatchInfo.getKernel()->setUnifiedMemorySyncRequirement(true);
    }
}
} // namespace NEO
