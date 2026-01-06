/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "gtest/gtest.h"

using namespace NEO;

struct WaitPkgFixture {
    using CpuIdFuncT = void (*)(int *, int);

#ifdef SUPPORTS_WAITPKG
    constexpr static bool expectedWaitpkgSupport = SUPPORTS_WAITPKG;
#else
    constexpr static bool expectedWaitpkgSupport = false;
#endif

    void setUp() {
        mockCpuInfo = getMockCpuInfo(CpuInfo::getInstance());

        backupCpuInfo = std::make_unique<VariableBackup<MockCpuInfo>>(mockCpuInfo);
        backupWaitpkgSupport = std::make_unique<VariableBackup<bool>>(&WaitUtils::waitpkgSupport);
        backupWaitpkgUse = std::make_unique<VariableBackup<WaitUtils::WaitpkgUse>>(&WaitUtils::waitpkgUse);
        backupWaitpkgCounter = std::make_unique<VariableBackup<uint64_t>>(&WaitUtils::waitpkgCounterValue);
        backupWaitpkgControl = std::make_unique<VariableBackup<uint32_t>>(&WaitUtils::waitpkgControlValue);
        backupWaitCount = std::make_unique<VariableBackup<uint32_t>>(&WaitUtils::waitCount);

        savedCpuIdFunc = CpuInfo::cpuidFunc;
        mockCpuInfo->features = CpuInfo::featureNone;
    }

    void tearDown() {
        CpuInfo::cpuidFunc = savedCpuIdFunc;
    }

    DebugManagerStateRestore restore;
    std::unique_ptr<VariableBackup<MockCpuInfo>> backupCpuInfo;
    std::unique_ptr<VariableBackup<bool>> backupWaitpkgSupport;
    std::unique_ptr<VariableBackup<WaitUtils::WaitpkgUse>> backupWaitpkgUse;
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

extern std::atomic_uint32_t tpauseCounter;

extern uint64_t rdtscRetValue;
extern unsigned char umwaitRetValue;

extern std::function<void()> controlUmwait;
} // namespace CpuIntrinsicsTests

template <int32_t waitpkgUse>
struct WaitPkgEnabledFixture : public WaitPkgFixture {
    void setUp() {
        WaitPkgFixture::setUp();

        debugManager.flags.EnableWaitpkg.set(waitpkgUse);

        CpuInfo::cpuidFunc = mockCpuidEnableAll;
        WaitUtils::waitpkgSupport = true;

        WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);

        CpuIntrinsicsTests::lastUmwaitCounter = 0;
        CpuIntrinsicsTests::lastUmwaitControl = 0;
        CpuIntrinsicsTests::umwaitCounter = 0;
        CpuIntrinsicsTests::lastUmonitorPtr = 0;
        CpuIntrinsicsTests::umonitorCounter = 0;
        CpuIntrinsicsTests::rdtscCounter = 0;
        CpuIntrinsicsTests::tpauseCounter = 0;

        backupCpuIntrinsicsRdtscRetValue = std::make_unique<VariableBackup<uint64_t>>(&CpuIntrinsicsTests::rdtscRetValue);
        backupCpuIntrinsicsUmwaitRetValue = std::make_unique<VariableBackup<unsigned char>>(&CpuIntrinsicsTests::umwaitRetValue);
        backupCpuIntrinsicsControlUmwait = std::make_unique<VariableBackup<std::function<void()>>>(&CpuIntrinsicsTests::controlUmwait);
    }

    std::unique_ptr<VariableBackup<uint64_t>> backupCpuIntrinsicsRdtscRetValue;
    std::unique_ptr<VariableBackup<unsigned char>> backupCpuIntrinsicsUmwaitRetValue;
    std::unique_ptr<VariableBackup<std::function<void()>>> backupCpuIntrinsicsControlUmwait;
};

using WaitPkgTest = Test<WaitPkgFixture>;
using WaitPkgEnabledTest = Test<WaitPkgEnabledFixture<1>>;
using WaitPkgTpauseEnabledTest = Test<WaitPkgEnabledFixture<2>>;

TEST_F(WaitPkgTest, givenDefaultSettingsAndWaitpkgSupportTrueWhenWaitInitializedThenWaitPkgNotEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::uninitialized);
    EXPECT_EQ(20, WaitUtils::waitPkgThresholdInMicroSeconds);

    EXPECT_EQ(expectedWaitpkgSupport, WaitUtils::waitpkgSupport);

    WaitUtils::waitpkgSupport = true;

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportFalseWhenWaitInitializedThenWaitPkgNotEnabled) {
    WaitUtils::waitpkgSupport = false;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    EXPECT_EQ(20, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenDisabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedThenWaitPkgNotEnabled) {
    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(0);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    EXPECT_EQ(20, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedAndCpuDoesNotSupportOperandThenWaitPkgNotEnabled) {
    CpuInfo::cpuidFunc = mockCpuidFunctionNotAvailableDisableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(WaitUtils::defaultWaitCount, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    EXPECT_EQ(20, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedAndCpuSupportsOperandThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);

    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::umonitorAndUmwait);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSetToTpauseAndWaitpkgSupportTrueWhenWaitInitializedAndCpuSupportsOperandThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(2);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);

    EXPECT_EQ(1u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenWaitpkgSupportTrueWhenCreateExecutionEnvironmentThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    MockExecutionEnvironment executionEnvironment;

    EXPECT_EQ(1u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenWaitpkgSupportTrueWhenCreateDevicesThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    auto executionEnvironment = new NEO::ExecutionEnvironment;
    executionEnvironment->incRefInternal();
    NEO::DeviceFactory::createDevices(*executionEnvironment);
    executionEnvironment->decRefInternal();

    EXPECT_EQ(1u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenWaitpkgSupportTrueWhenPrepareDeviceEnvironmentsThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    ExecutionEnvironment executionEnvironment;
    EXPECT_TRUE(NEO::DeviceFactory::prepareDeviceEnvironments(executionEnvironment));

    EXPECT_EQ(1u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSetToTpauseAndWaitpkgThresholdAndWaitpkgSupportTrueWhenWaitInitializedAndCpuSupportsOperandThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(2);
    debugManager.flags.WaitpkgThreshold.set(56789);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);

    EXPECT_EQ(1u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(56789, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenEnabledSetToTrueAndWaitpkgSupportTrueWhenWaitInitializedAndCpuSupportsOperandThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    WaitUtils::init(WaitUtils::WaitpkgUse::umonitorAndUmwait, *defaultHwInfo);

    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::umonitorAndUmwait);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenEnabledSetToTpauseAndWaitpkgSupportTrueWhenWaitInitializedAndCpuSupportsOperandThenWaitPkgEnabled) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    WaitUtils::init(WaitUtils::WaitpkgUse::tpause, *defaultHwInfo);

    EXPECT_EQ(1u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenFullyEnabledWaitPkgAndOverrideCounterValueWhenWaitInitializedThenNewCounterValueSet) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);
    debugManager.flags.WaitpkgCounterValue.set(1234);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(1234u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::umonitorAndUmwait);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenFullyEnabledWaitPkgAndOverrideControlValueWhenWaitInitializedThenNewControlValueSet) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    debugManager.flags.EnableWaitpkg.set(1);
    debugManager.flags.WaitpkgControlValue.set(1);

    WaitUtils::init(WaitUtils::WaitpkgUse::noUse, *defaultHwInfo);
    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(1u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::umonitorAndUmwait);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgTest, givenEnabledWaitPkgSettingsAndWaitpkgSupportTrueWhenWaitInitializedTwiceThenInitOnce) {
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    WaitUtils::waitpkgSupport = true;

    WaitUtils::init(WaitUtils::WaitpkgUse::umonitorAndUmwait, *defaultHwInfo);

    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::umonitorAndUmwait);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);

    debugManager.flags.WaitpkgControlValue.set(1);

    WaitUtils::init(WaitUtils::WaitpkgUse::umonitorAndUmwait, *defaultHwInfo);

    EXPECT_EQ(0u, WaitUtils::waitCount);
    EXPECT_EQ(12000u, WaitUtils::waitpkgCounterValue);
    EXPECT_EQ(0u, WaitUtils::waitpkgControlValue);
    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::umonitorAndUmwait);
    EXPECT_EQ(defaultHwInfo->capabilityTable.isIntegratedDevice ? 20 : 28, WaitUtils::waitPkgThresholdInMicroSeconds);
}

TEST_F(WaitPkgEnabledTest, givenMonitoredAddressChangedWhenAddressMatchesPredicateValueThenWaitReturnsTrue) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;

    CpuIntrinsicsTests::rdtscRetValue = 1234;

    CpuIntrinsicsTests::controlUmwait = [&]() {
        CpuIntrinsicsTests::umwaitRetValue = 0;
        pollValue = 1;
    };

    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue, 0);
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

    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue, 0);
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

    bool ret = WaitUtils::waitFunction(&pollValue, expectedValue, 0);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, CpuIntrinsicsTests::rdtscCounter);

    EXPECT_EQ(1u, CpuIntrinsicsTests::umonitorCounter);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&pollValue), CpuIntrinsicsTests::lastUmonitorPtr);

    EXPECT_EQ(CpuIntrinsicsTests::rdtscRetValue + WaitUtils::waitpkgCounterValue, CpuIntrinsicsTests::lastUmwaitCounter);
    EXPECT_EQ(WaitUtils::waitpkgControlValue, CpuIntrinsicsTests::lastUmwaitControl);
    EXPECT_EQ(1u, CpuIntrinsicsTests::umwaitCounter);
}

TEST_F(WaitPkgEnabledTest, givenTimeElapsedSinceWaitStartedBelowThresholdWhenWaitThenDoNoTpause) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;
    int64_t timeElapsedSinceWaitStarted = 0u;
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);

    WaitUtils::waitFunction(&pollValue, expectedValue, timeElapsedSinceWaitStarted);
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);
}

TEST_F(WaitPkgEnabledTest, givenTimeElapsedSinceWaitStartedAboveThresholdWhenWaitThenDoNoTpause) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;
    int64_t timeElapsedSinceWaitStarted = 5000u;
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);

    WaitUtils::waitFunction(&pollValue, expectedValue, timeElapsedSinceWaitStarted);
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);
}

TEST_F(WaitPkgTpauseEnabledTest, givenTimeElapsedSinceWaitStartedBelowThresholdWhenWaitThenDoNoTpause) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;
    int64_t timeElapsedSinceWaitStarted = 0u;
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);

    WaitUtils::waitFunction(&pollValue, expectedValue, timeElapsedSinceWaitStarted);
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);
}

TEST_F(WaitPkgTpauseEnabledTest, givenTimeElapsedSinceWaitStartedAboveThresholdWhenWaitThenDoTpause) {
    volatile TagAddressType pollValue = 0u;
    TaskCountType expectedValue = 1;
    int64_t timeElapsedSinceWaitStarted = 5000u;
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 0u);

    WaitUtils::waitFunction(&pollValue, expectedValue, timeElapsedSinceWaitStarted);
    EXPECT_EQ(CpuIntrinsicsTests::tpauseCounter, 1u);
}