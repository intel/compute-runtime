/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
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

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    L0::ult::MockDeviceImp deviceImp(neoDevice);
    auto debugSession = std::make_unique<L0::ult::DebugSessionMock>(zet_debug_config_t{0x1234}, &deviceImp);

    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

    EuThread::ThreadId thread0Eu0 = {0, 0, 0, 0, 0};
    EuThread::ThreadId thread0Eu1 = {0, 0, 0, 1, 0};
    EuThread::ThreadId thread2Subslice1 = {0, 0, 1, 0, 2};
    EuThread::ThreadId thread2EuLastSubslice1 = {0, 0, 1, hwInfo.gtSystemInfo.MaxEuPerSubSlice - 1, 2};

    const uint32_t ptss = 128;
    const auto &productHelper = neoDevice->getProductHelper();

    const uint32_t ratio = productHelper.getThreadEuRatioForScratch(hwInfo) / numThreadsPerEu;

    EXPECT_EQ(2u, productHelper.getThreadEuRatioForScratch(hwInfo) / numThreadsPerEu);

    auto offset = debugSession->getPerThreadScratchOffset(ptss, thread0Eu0);
    EXPECT_EQ(0u, offset);

    offset = debugSession->getPerThreadScratchOffset(ptss, thread0Eu1);
    EXPECT_EQ(ptss * numThreadsPerEu * ratio, offset);

    offset = debugSession->getPerThreadScratchOffset(ptss, thread2Subslice1);
    EXPECT_EQ((thread2Subslice1.subslice * hwInfo.gtSystemInfo.MaxEuPerSubSlice * numThreadsPerEu * ratio + thread2Subslice1.thread) * ptss, offset);

    offset = debugSession->getPerThreadScratchOffset(ptss, thread2EuLastSubslice1);
    EXPECT_EQ(((thread2EuLastSubslice1.subslice * hwInfo.gtSystemInfo.MaxEuPerSubSlice + thread2EuLastSubslice1.eu) * numThreadsPerEu * ratio + thread2EuLastSubslice1.thread) * ptss, offset);
}