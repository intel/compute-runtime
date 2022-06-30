/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "shared/source/os_interface/linux/os_inc.h"

#include <cpuid.h>
#include <fstream>

namespace NEO {

void cpuidLinuxWrapper(int cpuInfo[4], int functionId) {
    __cpuid_count(functionId, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

void cpuidexLinuxWrapper(int *cpuInfo, int functionId, int subfunctionId) {
    __cpuid_count(functionId, subfunctionId, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

void getCpuFlagsLinux(std::string &cpuFlags) {
    std::ifstream cpuinfo(std::string(Os::sysFsProcPathPrefix) + "/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.substr(0, 5) == "flags") {
            cpuFlags = line;
            break;
        }
    }
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidexLinuxWrapper;
void (*CpuInfo::cpuidFunc)(int[4], int) = cpuidLinuxWrapper;
void (*CpuInfo::getCpuFlagsFunc)(std::string &) = getCpuFlagsLinux;

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
