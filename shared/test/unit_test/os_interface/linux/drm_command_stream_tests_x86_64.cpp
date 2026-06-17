/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "gtest/gtest.h"

using namespace NEO;

HWTEST2_TEMPLATED_F(DrmCommandStreamUllsLightTest, givenDirectSubmissionLightWhenInitThenUseTpauseWaitpkg, IsXeLpg) {
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse);
    VariableBackup<uint64_t> backupWaitpkgCounterValue(&WaitUtils::waitpkgCounterValue);
    VariableBackup<uint64_t> backupCounterValueForEventHostSync(&WaitUtils::counterValueForEventHostSync);
    VariableBackup<int64_t> backupWaitPkgThreshold(&WaitUtils::waitPkgThresholdInMicroSeconds);
    VariableBackup<int64_t> backupWaitPkgThresholdForEventHostSync(&WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds);
    VariableBackup<bool> backupWaitpkgSupport(&WaitUtils::waitpkgSupport, true);
    VariableBackup<void (*)(int *, int)> backupCpuidFunc(&CpuInfo::cpuidFunc, mockCpuidEnableAll);

    auto mockCpuInfo = getMockCpuInfo(CpuInfo::getInstance());
    VariableBackup<MockCpuInfo> cpuInfoBackup(mockCpuInfo);
    mockCpuInfo->featuresDetected = false;

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->stopDirectSubmission(false, false);
    testedCsr->directSubmission.reset();

    csr->initDirectSubmission();

    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(WaitUtils::waitpkgCounterValue, WaitUtils::defaultCounterValueForUllsLight);
    EXPECT_EQ(WaitUtils::counterValueForEventHostSync, WaitUtils::defaultCounterValueForEventHostSync);
    EXPECT_EQ(WaitUtils::waitPkgThresholdInMicroSeconds, WaitUtils::defaultWaitPkgThresholdForUllsLightInMicroSeconds);
    EXPECT_EQ(WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds, WaitUtils::defaultWaitPkgThresholdForWddmEventHostSyncInMicroSeconds);
}

HWTEST2_TEMPLATED_F(DrmCommandStreamUllsLightTest, givenWaitpkgParamsSetByDebugVariablesAndDirectSubmissionLightWhenInitThenUseTpauseWaitpkg, IsXeLpg) {
    DebugManagerStateRestore restorer;
    debugManager.flags.WaitpkgThreshold.set(35);
    debugManager.flags.WaitpkgCounterValue.set(1000);

    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse);
    VariableBackup<uint64_t> backupWaitpkgCounterValue(&WaitUtils::waitpkgCounterValue);
    VariableBackup<uint64_t> backupCounterValueForEventHostSync(&WaitUtils::counterValueForEventHostSync);
    VariableBackup<int64_t> backupWaitPkgThreshold(&WaitUtils::waitPkgThresholdInMicroSeconds);
    VariableBackup<int64_t> backupWaitPkgThresholdForEventHostSync(&WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds);
    VariableBackup<bool> backupWaitpkgSupport(&WaitUtils::waitpkgSupport, true);
    VariableBackup<void (*)(int *, int)> backupCpuidFunc(&CpuInfo::cpuidFunc, mockCpuidEnableAll);

    auto mockCpuInfo = getMockCpuInfo(CpuInfo::getInstance());
    VariableBackup<MockCpuInfo> cpuInfoBackup(mockCpuInfo);
    mockCpuInfo->featuresDetected = false;

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->stopDirectSubmission(false, false);
    testedCsr->directSubmission.reset();

    csr->initDirectSubmission();

    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);
    EXPECT_EQ(WaitUtils::waitpkgCounterValue, 1000u);
    EXPECT_EQ(WaitUtils::counterValueForEventHostSync, 1000u);
    EXPECT_EQ(WaitUtils::waitPkgThresholdInMicroSeconds, 35);
    EXPECT_EQ(WaitUtils::waitPkgThresholdForEventHostSyncInMicroSeconds, 35);
}
