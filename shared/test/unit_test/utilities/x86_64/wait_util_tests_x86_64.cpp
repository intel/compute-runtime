/*
 * Copyright (C) 2023-2024 Intel Corporation
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
        backupWaitpkgCounter = std::make_unique<VariableBackup<uint64_t>>(&WaitUtils::waitpkgCounterValue);
        backupWaitpkgControl = std::make_unique<VariableBackup<uint32_t>>(&WaitUtils::waitpkgControlValue);
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

namespace CpuIntrinsicsTests {
extern std::atomic<uint64_t> lastUmwaitCounter;
extern std::atomic<unsigned int> lastUmwaitControl;
extern std::atomic<uint32_t> umwaitCounter;

extern std::atomic<uintptr_t> lastUmonitorPtr;
extern std::atomic<uint32_t> umonitorCounter;

extern std::atomic<uint32_t> rdtscCounter;

extern uint64_t rdtscRetValue;
extern unsigned char umwaitRetValue;

extern std::function<void()> controlUmwait;
} // namespace CpuIntrinsicsTests

struct WaitPkgEnabledFixture : public WaitPkgFixture {
    void setUp() {
        WaitPkgFixture::setUp();

        debugManager.flags.EnableWaitpkg.set(1);

        CpuInfo::cpuidFunc = mockCpuidEnableAll;
        WaitUtils::waitpkgSupport = true;

        WaitUtils::init();

        CpuIntrinsicsTests::lastUmwaitCounter = 0;
        CpuIntrinsicsTests::lastUmwaitControl = 0;
        CpuIntrinsicsTests::umwaitCounter = 0;
        CpuIntrinsicsTests::lastUmonitorPtr = 0;
        CpuIntrinsicsTests::umonitorCounter = 0;
        CpuIntrinsicsTests::rdtscCounter = 0;

        backupCpuIntrinsicsRdtscRetValue = std::make_unique<VariableBackup<uint64_t>>(&CpuIntrinsicsTests::rdtscRetValue);
        backupCpuIntrinsicsUmwaitRetValue = std::make_unique<VariableBackup<unsigned char>>(&CpuIntrinsicsTests::umwaitRetValue);
        backupCpuIntrinsicsControlUmwait = std::make_unique<VariableBackup<std::function<void()>>>(&CpuIntrinsicsTests::controlUmwait);
    }

    std::unique_ptr<VariableBackup<uint64_t>> backupCpuIntrinsicsRdtscRetValue;
    std::unique_ptr<VariableBackup<unsigned char>> backupCpuIntrinsicsUmwaitRetValue;
    std::unique_ptr<VariableBackup<std::function<void()>>> backupCpuIntrinsicsControlUmwait;
};

using WaitPkgTest = Test<WaitPkgFixture>;
using WaitPkgEnabledTest = Test<WaitPkgEnabledFixture>;

TEST_F(WaitPkgTest, givenDefaultSettingsAndWaitpkgSupportTrueWhenWaitInitializedThenWaitPkgNotEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);

    EXPECT_EQ(expectedWaitpkgSupport, WaitUtils::waitpkgSupport);

    WaitUtils::waitpkgSupport = true;

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportFalseWhenWaitInitializedThenWaitPkgNotEnabled) {
    WaitUtils::waitpkgSupport = false;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenDisabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedThenWaitPkgNotEnabled) {
    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(0);

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedAndCpuDoesNotSupportOperandThenWaitPkgNotEnabled) {
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init();
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_FALSE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedAndCpuSupportsOperandThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init();

    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_TRUE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenFullyEnabledWaitPkgAndOverrideCounterValueWhenWaitInitializedThenNewCounterValueSet) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);
    debugManager.flags.WaitpkgCounterValue.set(1234);

    WaitUtils::init();
    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(1234u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_TRUE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgTest, givenFullyEnabledWaitPkgAndOverrideControlValueWhenWaitInitializedThenNewControlValueSet) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);
    debugManager.flags.WaitpkgControlValue.set(1);

    WaitUtils::init();
    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(10000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(1u, WaitUtils::waitpkgControlValue);
    EXPECT_TRUE(WaitUtils::waitpkgUse);
}

TEST_F(WaitPkgEnabledTest, givenMonitoredAddressChangedWhenAddressMatchesPredicateValueThenWaitReturnsTrue) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;

    CpuIntrinsicsTests::rdtscRetValue = 1234;

    CpuIntrinsicsTests::controlUmwait = [&]() {
        CpuIntrinsicsTests::umwaitRetValue = 0;
        pollValue = 1;
    };

    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, CpuIntrinsicsTests::rdtscCounter);

    EXPECT_EQ(1u, CpuIntrinsicsTests::umonitorCounter);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&pollValue), CpuIntrinsicsTests::lastUmonitorPtr);

    EXPECT_EQ(CpuIntrinsicsTests::rdtscRetValue + WaitUtils::waitpkgCounterValue, CpuIntrinsicsTests::lastUmwaitCounter);
    EXPECT_EQ(WaitUtils::waitpkgControlValue, CpuIntrinsicsTests::lastUmwaitControl);
    EXPECT_EQ(1u, CpuIntrinsicsTests::umwaitCounter);
}

TEST_F(WaitPkgEnabledTest, givenMonitoredAddressNotChangesWhenMonitorTimeoutsThenWaitReturnsFalse) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;

    CpuIntrinsicsTests::rdtscRetValue = 2500;
    CpuIntrinsicsTests::umwaitRetValue = 1;

    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, CpuIntrinsicsTests::rdtscCounter);

    EXPECT_EQ(1u, CpuIntrinsicsTests::umonitorCounter);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&pollValue), CpuIntrinsicsTests::lastUmonitorPtr);

    EXPECT_EQ(CpuIntrinsicsTests::rdtscRetValue + WaitUtils::waitpkgCounterValue, CpuIntrinsicsTests::lastUmwaitCounter);
    EXPECT_EQ(WaitUtils::waitpkgControlValue, CpuIntrinsicsTests::lastUmwaitControl);
    EXPECT_EQ(1u, CpuIntrinsicsTests::umwaitCounter);
}

TEST_F(WaitPkgEnabledTest, givenMonitoredAddressChangedWhenAddressNotMatchesPredicateValueThenWaitReturnsFalse) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;

    CpuIntrinsicsTests::rdtscRetValue = 3700;
    CpuIntrinsicsTests::umwaitRetValue = 0;

    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, CpuIntrinsicsTests::rdtscCounter);

    EXPECT_EQ(1u, CpuIntrinsicsTests::umonitorCounter);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&pollValue), CpuIntrinsicsTests::lastUmonitorPtr);

    EXPECT_EQ(CpuIntrinsicsTests::rdtscRetValue + WaitUtils::waitpkgCounterValue, CpuIntrinsicsTests::lastUmwaitCounter);
    EXPECT_EQ(WaitUtils::waitpkgControlValue, CpuIntrinsicsTests::lastUmwaitControl);
    EXPECT_EQ(1u, CpuIntrinsicsTests::umwaitCounter);
}
