/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CpuInfoTest, giveFunctionIsNotAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    void (*defaultCpuidFunc)(int *, int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureWaitPkg));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, giveFunctionIsAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    void (*defaultCpuidFunc)(int *, int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidFunctionAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureWaitPkg));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, whenFeatureIsSupportedThenMaskBitIsOn) {
    void (*defaultCpuidFunc)(int *, int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureWaitPkg));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, WhenGettingVirtualAddressSizeThenCorrectResultIsReturned) {
    void (*defaultCpuidFunc)(int *, int) = CpuInfo::cpuidFunc;
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
