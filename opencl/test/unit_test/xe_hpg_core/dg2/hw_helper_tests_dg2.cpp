/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using HwHelperTestsDg2 = HwHelperTest;

DG2TEST_F(HwHelperTestsDg2, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping,
    };

    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping <= REVISION_B) {
            if (stepping == REVISION_A0) {
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
            } else if (stepping == REVISION_A1) {
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
            } else { // REVISION_B
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
            }
        } else {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        }

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A1, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_A1, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_B, hardwareInfo));

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo));
    }
}

DG2TEST_F(HwHelperTestsDg2, givenDg2WhenSetForceNonCoherentThenProperFlagSet) {
    using FORCE_NON_COHERENT = typename FamilyType::STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    auto stateComputeMode = FamilyType::cmdInitStateComputeMode;
    auto properties = StateComputeModeProperties{};

    properties.isCoherencyRequired.set(false);
    hwInfoConfig->setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(XE_HPG_COREFamily::stateComputeModeForceNonCoherentMask, stateComputeMode.getMaskBits());

    properties.isCoherencyRequired.set(true);
    hwInfoConfig->setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(XE_HPG_COREFamily::stateComputeModeForceNonCoherentMask, stateComputeMode.getMaskBits());
}

DG2TEST_F(HwHelperTestsDg2, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 7, 1), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}
