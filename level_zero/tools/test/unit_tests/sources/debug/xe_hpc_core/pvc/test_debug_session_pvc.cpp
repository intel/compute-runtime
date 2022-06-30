/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

using namespace L0;

using PVCDebugSession = ::testing::Test;

PVCTEST_F(PVCDebugSession, givenPVCRevId3WhenGettingPerThreadScratchOffsetThenPerThreadScratchOffsetIsBasedOnThreadEuRatio) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.usRevId = 3;

    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(productFamily);

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    L0::ult::Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<L0::ult::DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);

    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

    EuThread::ThreadId thread0Eu0 = {0, 0, 0, 0, 0};
    EuThread::ThreadId thread0Eu1 = {0, 0, 0, 1, 0};
    EuThread::ThreadId thread2Subslice1 = {0, 0, 1, 0, 2};

    const uint32_t ptss = 128;
    const uint32_t adjustedPtss = hwInfoConfig.getThreadEuRatioForScratch(hwInfo) / numThreadsPerEu * ptss;

    EXPECT_EQ(2u, hwInfoConfig.getThreadEuRatioForScratch(hwInfo) / numThreadsPerEu);

    auto offset = debugSession->getPerThreadScratchOffset(ptss, thread0Eu0);
    EXPECT_EQ(0u, offset);

    offset = debugSession->getPerThreadScratchOffset(ptss, thread0Eu1);
    EXPECT_EQ(adjustedPtss * numThreadsPerEu, offset);

    offset = debugSession->getPerThreadScratchOffset(ptss, thread2Subslice1);
    EXPECT_EQ(2 * adjustedPtss + adjustedPtss * hwInfo.gtSystemInfo.MaxEuPerSubSlice * numThreadsPerEu, offset);
}