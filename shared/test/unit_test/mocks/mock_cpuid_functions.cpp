/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

void mockCpuidEnableAll(int *cpuInfo, int functionId) {
    cpuInfo[0] = -1;
    cpuInfo[1] = -1;
    cpuInfo[2] = -1;
    cpuInfo[3] = -1;
}

void mockCpuidFunctionAvailableDisableAll(int *cpuInfo, int functionId) {
    cpuInfo[0] = -1;
    cpuInfo[1] = 0;
    cpuInfo[2] = 0;
    cpuInfo[3] = 0;
}

void mockCpuidFunctionNotAvailableDisableAll(int *cpuInfo, int functionId) {
    cpuInfo[0] = 0;
    cpuInfo[1] = 0;
    cpuInfo[2] = 0;
    cpuInfo[3] = 0;
}

void mockCpuidReport36BitVirtualAddressSize(int *cpuInfo, int functionId) {
    if (static_cast<unsigned int>(functionId) == 0x80000008) {
        cpuInfo[0] = 36 << 8;
        cpuInfo[1] = 0;
        cpuInfo[2] = 0;
        cpuInfo[3] = 0;
    } else {
        mockCpuidEnableAll(cpuInfo, functionId);
    }
}

void mockCpuidEnableAllExceptAvx512FoundationBit(int *cpuInfo, int functionId) {
    constexpr unsigned int extendedFeatures = 0x7;
    mockCpuidEnableAll(cpuInfo, functionId);
    if (static_cast<unsigned int>(functionId) == extendedFeatures) {
        cpuInfo[1] &= ~(1 << 16);
    }
}

void mockCpuidEnableAllExceptAvx2Bit(int *cpuInfo, int functionId) {
    constexpr unsigned int extendedFeatures = 0x7;
    mockCpuidEnableAll(cpuInfo, functionId);
    if (static_cast<unsigned int>(functionId) == extendedFeatures) {
        cpuInfo[1] &= ~(1 << 5);
    }
}

uint64_t mockXgetbvEnableAll(uint32_t) {
    return ~static_cast<uint64_t>(0);
}

uint64_t mockXgetbvDisableAll(uint32_t) {
    return 0;
}
