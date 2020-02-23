/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include <intrin.h>

namespace NEO {

void cpuid_windows_wrapper(int cpuInfo[4], int functionId) {
    __cpuid(cpuInfo, functionId);
}

void cpuidex_windows_wrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuidex(cpuInfo, functionId, subfunctionId);
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidex_windows_wrapper;
void (*CpuInfo::cpuidFunc)(int[4], int) = cpuid_windows_wrapper;

const CpuInfo CpuInfo::instance;

void CpuInfo::cpuid(
    uint32_t cpuInfo[4],
    uint32_t functionId) const {
    cpuidFunc(reinterpret_cast<int *>(cpuInfo), functionId);
}

void CpuInfo::cpuidex(
    uint32_t cpuInfo[4],
    uint32_t functionId,
    uint32_t subfunctionId) const {
    cpuidexFunc(reinterpret_cast<int *>(cpuInfo), functionId, subfunctionId);
}

} // namespace NEO
