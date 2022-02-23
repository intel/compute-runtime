/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "shared/source/os_interface/linux/os_inc.h"

#include <cstdint>
#include <fstream>

namespace NEO {

void cpuid_linux_wrapper(int cpuInfo[4], int functionId) {
}

void cpuidex_linux_wrapper(int *cpuInfo, int functionId, int subfunctionId) {
}

void get_cpu_flags_linux(std::string &cpuFlags) {
    std::ifstream cpuinfo(std::string(Os::sysFsProcPathPrefix) + "/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.substr(0, 5) == "flags") {
            cpuFlags = line;
            break;
        }
    }
}

void (*CpuInfo::cpuidexFunc)(int *, int, int) = cpuidex_linux_wrapper;
void (*CpuInfo::cpuidFunc)(int[4], int) = cpuid_linux_wrapper;
void (*CpuInfo::getCpuFlagsFunc)(std::string &) = get_cpu_flags_linux;

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
