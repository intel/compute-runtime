/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include <cpuid.h>

namespace NEO {

void cpuid_linux_wrapper(int cpuInfo[4], int functionId) {
    __cpuid_count(functionId, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

void cpuidex_linux_wrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuid_count(functionId, subfunctionId, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidex_linux_wrapper;
void (*CpuInfo::cpuidFunc)(int[4], int) = cpuid_linux_wrapper;

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
