/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/cpu_info.h"

#include <intrin.h>

namespace OCLRT {

void cpuidex_windows_wrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuidex(cpuInfo, functionId, subfunctionId);
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidex_windows_wrapper;

const CpuInfo CpuInfo::instance;

void CpuInfo::cpuid(
    uint32_t cpuInfo[4],
    uint32_t functionId) const {
    __cpuid(reinterpret_cast<int *>(cpuInfo), functionId);
}

void CpuInfo::cpuidex(
    uint32_t cpuInfo[4],
    uint32_t functionId,
    uint32_t subfunctionId) const {
    cpuidexFunc(reinterpret_cast<int *>(cpuInfo), functionId, subfunctionId);
}

} // namespace OCLRT
