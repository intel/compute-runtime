/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_info.h"

#include "opencl/source/program/kernel_info.h"

namespace NEO {

ProgramInfo::~ProgramInfo() {
    for (auto &kernelInfo : kernelInfos) {
        delete kernelInfo;
    }
    kernelInfos.clear();
}

size_t getMaxInlineSlmNeeded(const ProgramInfo &programInfo) {
    uint32_t ret = 0U;
    for (const auto &kernelInfo : programInfo.kernelInfos) {
        if (nullptr == kernelInfo->patchInfo.localsurface) {
            continue;
        }
        ret = std::max(ret, kernelInfo->patchInfo.localsurface->TotalInlineLocalMemorySize);
    }
    return ret;
}

bool requiresLocalMemoryWindowVA(const ProgramInfo &programInfo) {
    for (const auto &kernelInfo : programInfo.kernelInfos) {
        if (WorkloadInfo::undefinedOffset != kernelInfo->workloadInfo.localMemoryStatelessWindowStartAddressOffset) {
            return true;
        }
    }
    return false;
}

} // namespace NEO
