/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/cpu_info.h"

using namespace NEO;

struct MockCpuInfo : public NEO::CpuInfo {
    using CpuInfo::features;
};

inline MockCpuInfo *getMockCpuInfo(const NEO::CpuInfo &cpuInfo) {
    return static_cast<MockCpuInfo *>(const_cast<NEO::CpuInfo *>(&CpuInfo::getInstance()));
}

void mockCpuidEnableAll(int *cpuInfo, int functionId);

void mockCpuidFunctionAvailableDisableAll(int *cpuInfo, int functionId);

void mockCpuidFunctionNotAvailableDisableAll(int *cpuInfo, int functionId);

void mockCpuidReport36BitVirtualAddressSize(int *cpuInfo, int functionId);
