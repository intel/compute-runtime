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

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

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

DG2TEST_F(HwHelperTestsDg2, givenDifferentCLImageFormatsWhenCallingAllowImageCompressionThenCorrectValueReturned) {
    struct ImageFormatCompression {
        cl_image_format imageFormat;
        bool isCompressable;
    };
    const std::vector<ImageFormatCompression> imageFormats = {
        {{CL_LUMINANCE, CL_UNORM_INT8}, false},
        {{CL_LUMINANCE, CL_UNORM_INT16}, false},
        {{CL_LUMINANCE, CL_HALF_FLOAT}, false},
        {{CL_LUMINANCE, CL_FLOAT}, false},
        {{CL_INTENSITY, CL_UNORM_INT8}, false},
        {{CL_INTENSITY, CL_UNORM_INT16}, false},
        {{CL_INTENSITY, CL_HALF_FLOAT}, false},
        {{CL_INTENSITY, CL_FLOAT}, false},
        {{CL_A, CL_UNORM_INT16}, false},
        {{CL_A, CL_HALF_FLOAT}, false},
        {{CL_A, CL_FLOAT}, false},
        {{CL_R, CL_UNSIGNED_INT8}, true},
        {{CL_R, CL_UNSIGNED_INT16}, true},
        {{CL_R, CL_UNSIGNED_INT32}, true},
        {{CL_RG, CL_UNSIGNED_INT32}, true},
        {{CL_RGBA, CL_UNSIGNED_INT32}, true},
        {{CL_RGBA, CL_UNORM_INT8}, true},
        {{CL_RGBA, CL_UNORM_INT16}, true},
        {{CL_RGBA, CL_SIGNED_INT8}, true},
        {{CL_RGBA, CL_SIGNED_INT16}, true},
        {{CL_RGBA, CL_SIGNED_INT32}, true},
        {{CL_RGBA, CL_UNSIGNED_INT8}, true},
        {{CL_RGBA, CL_UNSIGNED_INT16}, true},
        {{CL_RGBA, CL_UNSIGNED_INT32}, true},
        {{CL_RGBA, CL_HALF_FLOAT}, true},
        {{CL_RGBA, CL_FLOAT}, true},
        {{CL_BGRA, CL_UNORM_INT8}, true},
        {{CL_R, CL_FLOAT}, true},
        {{CL_R, CL_UNORM_INT8}, true},
        {{CL_R, CL_UNORM_INT16}, true},
    };
    MockContext context;
    auto &clHwHelper = ClHwHelper::get(context.getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);

    for (const auto &format : imageFormats) {
        bool result = clHwHelper.allowImageCompression(format.imageFormat);
        EXPECT_EQ(format.isCompressable, result);
    }
}
