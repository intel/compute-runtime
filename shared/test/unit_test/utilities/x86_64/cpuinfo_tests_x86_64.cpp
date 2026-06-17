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
    using XgetbvFuncT = uint64_t (*)(uint32_t);
    void setUp() {
        defaultCpuidFunc = CpuInfo::cpuidFunc;
        defaultXgetbvFunc = CpuInfo::xgetbvFunc;
    }

    void tearDown() {
        CpuInfo::cpuidFunc = defaultCpuidFunc;
        CpuInfo::xgetbvFunc = defaultXgetbvFunc;
    }

    CpuIdFuncT defaultCpuidFunc;
    XgetbvFuncT defaultXgetbvFunc;
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

    CpuInfo testCpuInfo;

    StreamCapture capture;
    capture.captureStdout();
    auto addressSize = testCpuInfo.getVirtualAddressSize();
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(36u, addressSize);
    std::string expectedString = "CPUFlags:\nCLFlush: 1 Avx2: 1 Avx512: 1 WaitPkg: 1\nVirtual Address Size 36\n";
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
