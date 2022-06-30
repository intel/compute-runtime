/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "gtest/gtest.h"

using namespace NEO;

void mockCpuidEnableAll(int cpuInfo[4], int functionId) {
    cpuInfo[0] = -1;
    cpuInfo[1] = -1;
    cpuInfo[2] = -1;
    cpuInfo[3] = -1;
}

void mockCpuidFunctionAvailableDisableAll(int cpuInfo[4], int functionId) {
    cpuInfo[0] = -1;
    cpuInfo[1] = 0;
    cpuInfo[2] = 0;
    cpuInfo[3] = 0;
}

void mockCpuidFunctionNotAvailableDisableAll(int cpuInfo[4], int functionId) {
    cpuInfo[0] = 0;
    cpuInfo[1] = 0;
    cpuInfo[2] = 0;
    cpuInfo[3] = 0;
}

void mockCpuidReport36BitVirtualAddressSize(int cpuInfo[4], int functionId) {
    if (static_cast<uint32_t>(functionId) == 0x80000008) {
        cpuInfo[0] = 36 << 8;
        cpuInfo[1] = 0;
        cpuInfo[2] = 0;
        cpuInfo[3] = 0;
    } else {
        mockCpuidEnableAll(cpuInfo, functionId);
    }
}

TEST(CpuInfoTest, giveFunctionIsNotAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    void (*defaultCpuidFunc)(int[4], int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, giveFunctionIsAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    void (*defaultCpuidFunc)(int[4], int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidFunctionAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, whenFeatureIsSupportedThenMaskBitIsOn) {
    void (*defaultCpuidFunc)(int[4], int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, WhenGettingVirtualAddressSizeThenCorrectResultIsReturned) {
    void (*defaultCpuidFunc)(int[4], int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidReport36BitVirtualAddressSize;

    CpuInfo testCpuInfo;

    EXPECT_EQ(36u, testCpuInfo.getVirtualAddressSize());

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfo, WhenGettingCpuidexThenOperationSucceeds) {
    const CpuInfo &cpuInfo = CpuInfo::getInstance();

    uint32_t cpuRegsInfo[4];
    uint32_t subleaf = 0;
    cpuInfo.cpuidex(cpuRegsInfo, 4, subleaf);
}
