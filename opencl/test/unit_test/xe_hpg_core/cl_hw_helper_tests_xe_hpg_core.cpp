/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using ClHwHelperTestsXeHpgCore = ::testing::Test;

using namespace NEO;

XE_HPG_CORETEST_F(ClHwHelperTestsXeHpgCore, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 7, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
    } else {
        EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 7, 1), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
    }
}

XE_HPG_CORETEST_F(ClHwHelperTestsXeHpgCore, givenGenHelperWhenKernelArgumentIsNotPureStatefulThenRequireNonAuxMode) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : ::testing::Bool()) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;

        EXPECT_EQ(!argAsPtr.isPureStateful(), clHwHelper.requiresNonAuxMode(argAsPtr, *defaultHwInfo));
    }
}

XE_HPG_CORETEST_F(ClHwHelperTestsXeHpgCore, givenGenHelperWhenEnableStatelessCompressionThenDontRequireNonAuxMode) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : ::testing::Bool()) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_FALSE(clHwHelper.requiresNonAuxMode(argAsPtr, *defaultHwInfo));
    }
}

XE_HPG_CORETEST_F(ClHwHelperTestsXeHpgCore, givenGenHelperWhenCheckAuxTranslationThenAuxResolvesIsRequired) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : ::testing::Bool()) {
        KernelInfo kernelInfo{};
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_EQ(!isPureStateful, clHwHelper.requiresAuxResolves(kernelInfo, *defaultHwInfo));
    }
}

XE_HPG_CORETEST_F(ClHwHelperTestsXeHpgCore, givenGenHelperWhenEnableStatelessCompressionThenAuxTranslationIsNotRequired) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clHwHelper.requiresAuxResolves(kernelInfo, *defaultHwInfo));
}

XE_HPG_CORETEST_F(ClHwHelperTestsXeHpgCore, givenDifferentCLImageFormatsWhenCallingAllowImageCompressionThenCorrectValueReturned) {
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
