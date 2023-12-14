/*
 * Copyright (C) 2023 Intel Corporation
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
