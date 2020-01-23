/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hardware_commands_helper.h"
#include "test.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

using KernelTgllpTests = ::testing::Test;

TGLLPTEST_F(KernelTgllpTests, GivenUseOffsetToSkipSetFFIDGPWorkaroundActiveWhenSettingKernelStartOffsetThenAdditionalOffsetIsSet) {
    const uint64_t defaultKernelStartOffset = 0;
    const uint64_t additionalOffsetDueToFfid = 0x1234;
    SPatchThreadPayload threadPayload{};
    threadPayload.OffsetToSkipSetFFIDGP = additionalOffsetDueToFfid;
    auto hwInfo = *platformDevices[0];

    unsigned short steppings[] = {REVISION_A0, REVISION_A1, REVISION_A3, REVISION_B};
    for (auto stepping : steppings) {

        hwInfo.platform.usRevId = stepping;
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        MockKernelWithInternals mockKernelWithInternals{*device};
        mockKernelWithInternals.kernelInfo.patchInfo.threadPayload = &threadPayload;

        for (auto isCcsUsed : ::testing::Bool()) {
            uint64_t kernelStartOffset = mockKernelWithInternals.mockKernel->getKernelStartOffset(false, false, isCcsUsed);

            if (stepping < REVISION_B && isCcsUsed) {
                EXPECT_EQ(defaultKernelStartOffset + additionalOffsetDueToFfid, kernelStartOffset);
            } else {
                EXPECT_EQ(defaultKernelStartOffset, kernelStartOffset);
            }
        }
    }
}