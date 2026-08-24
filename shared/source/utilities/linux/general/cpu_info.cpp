
/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "shared/source/os_interface/linux/os_inc.h"

#include <fstream>

namespace NEO {

void cpuidLinuxWrapper(int cpuInfo[4], int functionId) {
}

void cpuidexLinuxWrapper(int *cpuInfo, int functionId, int subfunctionId) {
}

void getCpuFlagsLinux(std::string &cpuFlags) {
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidexLinuxWrapper;
void (*CpuInfo::cpuidFunc)(int[4], int) = cpuidLinuxWrapper;
void (*CpuInfo::getCpuFlagsFunc)(std::string &) = getCpuFlagsLinux;

const CpuInfo CpuInfo::instance;

void CpuInfo::cpuid(
    uint32_t cpuInfo[4],
    uint32_t functionId) const {
}

void CpuInfo::cpuidex(
    uint32_t cpuInfo[4],
    uint32_t functionId,
    uint32_t subfunctionId) const {
}

} // namespace NEO