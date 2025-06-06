/*
 * Copyright (C) 2019-2025 Intel Corporation
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
    void setUp() {
        defaultCpuidFunc = CpuInfo::cpuidFunc;
    }

    void tearDown() {
        CpuInfo::cpuidFunc = defaultCpuidFunc;
    }

    CpuIdFuncT defaultCpuidFunc;
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

TEST_F(CpuInfoTest, whenFeatureIsSupportedThenMaskBitIsOn) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    CpuInfo testCpuInfo;

    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureAvX2));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureClflush));
    EXPECT_TRUE(testCpuInfo.isFeatureSupported(CpuInfo::featureWaitPkg));
}

TEST_F(CpuInfoTest, WhenGettingVirtualAddressSizeThenCorrectResultIsReturned) {
    CpuInfo::cpuidFunc = mockCpuidReport36BitVirtualAddressSize;

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

    CpuInfo testCpuInfo;

    StreamCapture capture;
    capture.captureStdout();
    auto addressSize = testCpuInfo.getVirtualAddressSize();
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(36u, addressSize);
    std::string expectedString = "CPUFlags:\nCLFlush: 1 Avx2: 1 WaitPkg: 1\nVirtual Address Size 36\n";
    EXPECT_STREQ(output.c_str(), expectedString.c_str());
}
