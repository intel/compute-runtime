/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "gtest/gtest.h"

using namespace NEO;

struct CpuInfoFixture {
    using CpuIdFuncT = void (*)(int *, int);
    using CpuIdexFuncT = void (*)(int *, int, int);
    using XgetbvFuncT = uint64_t (*)(uint32_t);
    using GetLastLevelCacheSizeFuncT = size_t (*)();
    void setUp() {
        defaultCpuidFunc = CpuInfo::cpuidFunc;
        defaultCpuidexFunc = CpuInfo::cpuidexFunc;
        defaultXgetbvFunc = CpuInfo::xgetbvFunc;
        defaultGetLastLevelCacheSizeFunc = CpuInfo::getLastLevelCacheSizeFunc;
    }

    void tearDown() {
        CpuInfo::cpuidFunc = defaultCpuidFunc;
        CpuInfo::cpuidexFunc = defaultCpuidexFunc;
        CpuInfo::xgetbvFunc = defaultXgetbvFunc;
        CpuInfo::getLastLevelCacheSizeFunc = defaultGetLastLevelCacheSizeFunc;
    }

    CpuIdFuncT defaultCpuidFunc;
    CpuIdexFuncT defaultCpuidexFunc;
    XgetbvFuncT defaultXgetbvFunc;
    GetLastLevelCacheSizeFuncT defaultGetLastLevelCacheSizeFunc;
};

using CpuInfoTest = Test<CpuInfoFixture>;

TEST_F(CpuInfoTest, giveFunctionIsNotAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureWaitPkg));
}

TEST_F(CpuInfoTest, giveFunctionIsAvailableWhenFeatureIsNotSupportedThenMaskBitIsOff) {
    CpuInfo::cpuidFunc = mockCpuidFunctionAvailableDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));
    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureWaitPkg));
}

TEST_F(CpuInfoTest, GivenOsXsaveEnabledButAvx2FeatureBitOffWhenDetectingThenAvx2BitIsOff) {
    CpuInfo::cpuidFunc = mockCpuidEnableAllExceptAvx2Bit;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
}

TEST_F(CpuInfoTest, whenFeatureIsSupportedThenMaskBitIsOn) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureWaitPkg));
}

TEST_F(CpuInfoTest, WhenGettingVirtualAddressSizeThenCorrectResultIsReturned) {
    CpuInfo::cpuidFunc = mockCpuidReport36BitVirtualAddressSize;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_EQ(36u, testCpuInfo.getVirtualAddressSize());
}

TEST_F(CpuInfoTest, WhenGettingLastLevelCacheSizeThenOperatingSystemReportedSizeIsReturned) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;
    CpuInfo::getLastLevelCacheSizeFunc = mockGetLastLevelCacheSize;

    CpuInfo testCpuInfo;

    EXPECT_EQ(mockLastLevelCacheSize, testCpuInfo.getLastLevelCacheSize());
}

TEST_F(CpuInfoTest, GivenCacheSizeUnavailableWhenGettingLastLevelCacheSizeThenZeroIsReturned) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;
    CpuInfo::getLastLevelCacheSizeFunc = mockGetLastLevelCacheSizeUnavailable;

    MockCpuInfo testCpuInfo;
    testCpuInfo.lastLevelCacheSize = MemoryConstants::megaByte;

    EXPECT_EQ(0u, testCpuInfo.getLastLevelCacheSize());
}

TEST_F(CpuInfoTest, GivenFeaturesAlreadyDetectedWhenGettingLastLevelCacheSizeThenDetectionIsNotRepeated) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;
    CpuInfo::getLastLevelCacheSizeFunc = mockGetLastLevelCacheSize;

    MockCpuInfo testCpuInfo;
    testCpuInfo.lastLevelCacheSize = MemoryConstants::kiloByte;
    testCpuInfo.featuresDetected = true;

    EXPECT_EQ(MemoryConstants::kiloByte, testCpuInfo.getLastLevelCacheSize());
}

TEST(CpuInfo, WhenGettingCpuidexThenOperationSucceeds) {
    const CpuInfo &cpuInfo = CpuInfo::getInstance();

    uint32_t cpuRegsInfo[4];
    uint32_t subleaf = 0;
    cpuInfo.cpuidex(cpuRegsInfo, 4, subleaf);
}

TEST_F(CpuInfoTest, GivenPrintCpuFlagsEnabledWhenGettingVirtualAddressSizeThenCpuFlagsAndAddressSizePrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintCpuFlags.set(true);

    CpuInfo::cpuidFunc = mockCpuidReport36BitVirtualAddressSize;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;
    CpuInfo::getLastLevelCacheSizeFunc = mockGetLastLevelCacheSize;

    CpuInfo testCpuInfo;

    StreamCapture capture;
    capture.captureStdout();
    auto addressSize = testCpuInfo.getVirtualAddressSize();
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(36u, addressSize);
    std::string expectedString = "CPUFlags:\nCLFlush: 1 Avx2: 1 Avx512: 1 WaitPkg: 1\nVirtual Address Size 36\nLast Level Cache Size " + std::to_string(mockLastLevelCacheSize) + "\n";
    EXPECT_STREQ(output.c_str(), expectedString.c_str());
}

TEST_F(CpuInfoTest, GivenCpuidFunctionUnavailableWhenDetectingThenAvx512BitIsOff) {
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;
    CpuInfo::xgetbvFunc = mockXgetbvDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX512));
}

TEST_F(CpuInfoTest, GivenCpuidFunctionAvailableButAvx512UnsupportedWhenDetectingThenAvx512BitIsOff) {
    CpuInfo::cpuidFunc = mockCpuidFunctionAvailableDisableAll;
    CpuInfo::xgetbvFunc = mockXgetbvDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX512));
}

TEST_F(CpuInfoTest, GivenAvx512SupportedWhenDetectingThenAvx512BitIsOn) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX512));
}

TEST_F(CpuInfoTest, GivenOsXsaveEnabledButAvx512FoundationBitOffWhenDetectingThenAvx512BitIsOff) {
    CpuInfo::cpuidFunc = mockCpuidEnableAllExceptAvx512FoundationBit;
    CpuInfo::xgetbvFunc = mockXgetbvEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX512));
}

TEST_F(CpuInfoTest, GivenAvx512FeatureBitsSetButOsXcr0MaskUnsetWhenDetectingThenAvx512BitIsOff) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;
    CpuInfo::xgetbvFunc = mockXgetbvDisableAll;

    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX512));
}

namespace {
void setPagingMode(MockCpuInfo &cpuInfo, uint32_t virtualAddressSize, bool la57Present) {
    cpuInfo.featuresDetected = true;
    cpuInfo.virtualAddressSize = virtualAddressSize;
    cpuInfo.cpuFlags = la57Present ? "la57" : "lm";
}
} // namespace

TEST(CpuInfoMaxCpuVirtualAddressTest, given4LevelPagingThenUserSpaceEndsAt47Bits) {
    MockCpuInfo cpuInfo;
    setPagingMode(cpuInfo, 48u, false);

    EXPECT_EQ(maxNBitValue(47), cpuInfo.getMaxCpuVirtualAddress());
}

TEST(CpuInfoMaxCpuVirtualAddressTest, given5LevelPagingEnabledThenUserSpaceEndsAt56Bits) {
    MockCpuInfo cpuInfo;
    setPagingMode(cpuInfo, 57u, true);

    EXPECT_EQ(maxNBitValue(56), cpuInfo.getMaxCpuVirtualAddress());
}

TEST(CpuInfoMaxCpuVirtualAddressTest, given5LevelPagingCapableCpuWhenLa57IsNotEnabledThenUserSpaceEndsAt47Bits) {
    MockCpuInfo cpuInfo;
    setPagingMode(cpuInfo, 57u, false);

    EXPECT_EQ(maxNBitValue(47), cpuInfo.getMaxCpuVirtualAddress());
}
