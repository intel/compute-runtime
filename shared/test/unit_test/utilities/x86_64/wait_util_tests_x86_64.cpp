/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "gtest/gtest.h"

using namespace NEO;

struct WaitPkgFixture {
    using CpuIdFuncT = void (*)(int *, int);

    struct MockCpuInfo : public NEO::CpuInfo {
        using CpuInfo::features;
    };

#ifdef SUPPORTS_WAITPKG
    constexpr static bool expectedWaitpkgSupport = SUPPORTS_WAITPKG;
#else
    constexpr static bool expectedWaitpkgSupport = false;
#endif

    void setUp() {
        mockCpuInfo = getMockCpuInfo(CpuInfo::getInstance());

        backupCpuInfo = std::make_unique<VariableBackup<MockCpuInfo>>(mockCpuInfo);
        backupWaitpkgSupport = std::make_unique<VariableBackup<bool>>(&WaitUtils::waitpkgSupport);
        backupWaitpkgUse = std::make_unique<VariableBackup<bool>>(&WaitUtils::waitpkgUse);
        backupWaitpkgCounter = std::make_unique<VariableBackup<uint64_t>>(&WaitUtils::counterValue);
        backupWaitpkgControl = std::make_unique<VariableBackup<uint32_t>>(&WaitUtils::controlValue);
        backupWaitCount = std::make_unique<VariableBackup<uint32_t>>(&WaitUtils::waitCount);

        savedCpuIdFunc = CpuInfo::cpuidFunc;
        mockCpuInfo->features = CpuInfo::featureNone;
    }

    void tearDown() {
        CpuInfo::cpuidFunc = savedCpuIdFunc;
    }

    MockCpuInfo *getMockCpuInfo(const NEO::CpuInfo &cpuInfo) {
        return static_cast<MockCpuInfo *>(const_cast<NEO::CpuInfo *>(&CpuInfo::getInstance()));
    }

    DebugManagerStateRestore restore;
    std::unique_ptr<VariableBackup<MockCpuInfo>> backupCpuInfo;
    std::unique_ptr<VariableBackup<CpuIdFuncT>> backupCpuIdFunc;
    std::unique_ptr<VariableBackup<bool>> backupWaitpkgSupport;
    std::unique_ptr<VariableBackup<bool>> backupWaitpkgUse;
    std::unique_ptr<VariableBackup<uint64_t>> backupWaitpkgCounter;
    std::unique_ptr<VariableBackup<uint32_t>> backupWaitpkgControl;
    std::unique_ptr<VariableBackup<uint32_t>> backupWaitCount;

    MockCpuInfo *mockCpuInfo = nullptr;
    CpuIdFuncT savedCpuIdFunc = nullptr;
};

using WaitPkgTest = Test<WaitPkgFixture>;

TEST_F(WaitPkgTest, givenDefaultSettingsAndWaitpkgSupportTrueWhenWaitInitializedThenWaitPkgNotEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::counterValue);
    EXPECT_EQ(0u, WaitUtils::controlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);

    EXPECT_EQ(expectedWaitpkgSupport, WaitUtils::waitpkgSupport);

    WaitUtils::waitpkgSupport = true;

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::counterValue);
    EXPECT_EQ(0u, WaitUtils::controlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportFalseWhenWaitInitializedThenWaitPkgNotEnabled) {
    WaitUtils::waitpkgSupport = false;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::counterValue);
    EXPECT_EQ(0u, WaitUtils::controlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenDisabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedThenWaitPkgNotEnabled) {
    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(0);

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::counterValue);
    EXPECT_EQ(0u, WaitUtils::controlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedAndCpuDoesNotSupportOperandThenWaitPkgNotEnabled) {
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::counterValue);
    EXPECT_EQ(0u, WaitUtils::controlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedAndCpuSupportsOperandThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init();

    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::counterValue);
    EXPECT_EQ(0u, WaitUtils::controlValue);
    EXPECT_TRUE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenFullyEnabledWaitPkgAndOverrideCounterValueWhenWaitInitializedThenNewCounterValueSet) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);
    debugManager.flags.WaitpkgCounterValue.set(1234);

    WaitUtils::init();
    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(1234u, WaitUtils::counterValue);
    EXPECT_EQ(0u, WaitUtils::controlValue);
    EXPECT_TRUE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenFullyEnabledWaitPkgAndOverrideControlValueWhenWaitInitializedThenNewControlValueSet) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);
    debugManager.flags.WaitpkgControlValue.set(1);

    WaitUtils::init();
    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::counterValue);
    EXPECT_EQ(1u, WaitUtils::controlValue);
    EXPECT_TRUE(WaitUtils::waitpkgUse);
}
