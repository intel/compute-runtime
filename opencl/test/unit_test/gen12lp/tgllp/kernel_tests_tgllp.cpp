/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using KernelTgllpTests = ::testing::Test;

TGLLPTEST_F(KernelTgllpTests, GivenUseOffsetToSkipSetFFIDGPWorkaroundActiveWhenSettingKernelStartOffsetThenAdditionalOffsetIsSet) {
    const uint64_t defaultKernelStartOffset = 0;
    const uint64_t additionalOffsetDueToFfid = 0x1234;
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    unsigned short steppings[] = {REVISION_A0, REVISION_A1};
    for (auto stepping : steppings) {

        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hwInfo);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        MockKernelWithInternals mockKernelWithInternals{*device};
        mockKernelWithInternals.kernelInfo.kernelDescriptor.entryPoints.skipSetFFIDGP = additionalOffsetDueToFfid;

        for (auto isCcsUsed : ::testing::Bool()) {
            uint64_t kernelStartOffset = mockKernelWithInternals.mockKernel->getKernelStartOffset(false, false, isCcsUsed);

            if (stepping == REVISION_A0 && isCcsUsed) {
                EXPECT_EQ(defaultKernelStartOffset + additionalOffsetDueToFfid, kernelStartOffset);
            } else {
                EXPECT_EQ(defaultKernelStartOffset, kernelStartOffset);
            }
        }
    }
}
