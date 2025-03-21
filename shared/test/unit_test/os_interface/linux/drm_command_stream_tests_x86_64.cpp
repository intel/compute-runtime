/*
 * Copyright (C) 2025 Intel Corporation
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

HWTEST_TEMPLATED_F(DrmCommandStreamDirectSubmissionTest, givenDirectSubmissionLightWhenInitThenUseTpauseWaitpkg) {
    using CpuIdFuncT = void (*)(int *, int);

    auto mockCpuInfo = getMockCpuInfo(CpuInfo::getInstance());
    VariableBackup<MockCpuInfo> cpuInfoBackup(mockCpuInfo);
    mockCpuInfo->features = CpuInfo::featureNone;

    CpuIdFuncT savedCpuIdFunc = CpuInfo::cpuidFunc;
    CpuInfo::cpuidFunc = mockCpuidEnableAll;

    VariableBackup<bool> backupWaitpkgSupport(&WaitUtils::waitpkgSupport, true);

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testedCsr->directSubmission.reset();

    csr->initDirectSubmission();

    EXPECT_EQ(WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::tpause);

    CpuInfo::cpuidFunc = savedCpuIdFunc;
}