/*
 * Copyright (C) 2017-2020 Intel Corporation
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

TEST(CpuInfoTest, giveFunctionIsNotAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    void (*defaultCpuidFunc)(int[4], int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureFpu));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureCmov));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureMmx));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureFxsave));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSse));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE3));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSssE3));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE41));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE42));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureMovbe));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featurePopcnt));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featurePclmulqdq));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAes));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureF16C));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvx));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureRdrnd));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureFma));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureBmi));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureLzcnt));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureHle));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureRtm));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, giveFunctionIsAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    void (*defaultCpuidFunc)(int[4], int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidFunctionAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureFpu));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureCmov));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureMmx));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureFxsave));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSse));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE3));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSssE3));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE41));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE42));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureMovbe));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featurePopcnt));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featurePclmulqdq));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAes));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureF16C));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvx));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureRdrnd));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureFma));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureBmi));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureLzcnt));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureHle));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureRtm));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfoTest, whenFeatureIsSupportedThenMaskBitIsOn) {
    void (*defaultCpuidFunc)(int[4], int) = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureFpu));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureCmov));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureMmx));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureFxsave));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureSse));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE2));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE3));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureSssE3));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE41));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureSsE42));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureMovbe));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featurePopcnt));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featurePclmulqdq));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAes));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureF16C));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvx));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureRdrnd));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureFma));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureBmi));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureLzcnt));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureHle));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureRtm));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));

    CpuInfo::cpuidFunc = defaultCpuidFunc;
}

TEST(CpuInfo, cpuidex) {
    const CpuInfo &cpuInfo = CpuInfo::getInstance();

    uint32_t cpuRegsInfo[4];
    uint32_t subleaf = 0;
    cpuInfo.cpuidex(cpuRegsInfo, 4, subleaf);
}
