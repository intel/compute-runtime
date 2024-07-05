/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/xe_hpg_core/hw_info.h"
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_gfx_core_helper.h"

using ClGfxCoreHelperTestsXeHpgCore = Test<ClDeviceFixture>;

using namespace NEO;

XE_HPG_CORETEST_F(ClGfxCoreHelperTestsXeHpgCore, givenGenHelperWhenKernelArgumentIsNotPureStatefulThenRequireNonAuxMode) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto isPureStateful : ::testing::Bool()) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;

        EXPECT_EQ(!argAsPtr.isPureStateful(), clGfxCoreHelper.requiresNonAuxMode(argAsPtr));
    }
}

XE_HPG_CORETEST_F(ClGfxCoreHelperTestsXeHpgCore, givenGenHelperWhenEnableStatelessCompressionThenDontRequireNonAuxMode) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableStatelessCompression.set(1);

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto isPureStateful : ::testing::Bool()) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_FALSE(clGfxCoreHelper.requiresNonAuxMode(argAsPtr));
    }
}

XE_HPG_CORETEST_F(ClGfxCoreHelperTestsXeHpgCore, givenGenHelperWhenCheckAuxTranslationThenAuxResolvesIsRequired) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto isPureStateful : ::testing::Bool()) {
        KernelInfo kernelInfo{};
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_EQ(!isPureStateful, clGfxCoreHelper.requiresAuxResolves(kernelInfo));
    }
}

XE_HPG_CORETEST_F(ClGfxCoreHelperTestsXeHpgCore, givenGenHelperWhenEnableStatelessCompressionThenAuxTranslationIsNotRequired) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableStatelessCompression.set(1);

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clGfxCoreHelper.requiresAuxResolves(kernelInfo));
}

XE_HPG_CORETEST_F(ClGfxCoreHelperTestsXeHpgCore, givenDifferentCLImageFormatsWhenCallingAllowImageCompressionThenCorrectValueReturned) {
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

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (const auto &format : imageFormats) {
        bool result = clGfxCoreHelper.allowImageCompression(format.imageFormat);
        EXPECT_EQ(format.isCompressable, result);
    }
}

XE_HPG_CORETEST_F(GetDeviceInfoMemCapabilitiesTest, GivenValidParametersWhenGetDeviceInfoIsCalledThenClSuccessIsReturned) {
    auto productHelper = ProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::vector<TestParams> params = {
        {CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL, productHelper->getHostMemCapabilities(defaultHwInfo.get())},
        {CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
         (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL)},
        {CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL, 0}};

    check(params);
}
