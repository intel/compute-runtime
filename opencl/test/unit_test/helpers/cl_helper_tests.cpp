/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/cl_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <array>

using namespace NEO;

TEST(ClHelper, whenCallGetStringWithCmdTypeFunctionThenGetProperCmdTypeAsString) {
    std::array<std::string, 31> expected = {{"CL_COMMAND_NDRANGE_KERNEL",
                                             "CL_COMMAND_TASK",
                                             "CL_COMMAND_NATIVE_KERNEL",
                                             "CL_COMMAND_READ_BUFFER",
                                             "CL_COMMAND_WRITE_BUFFER",
                                             "CL_COMMAND_COPY_BUFFER",
                                             "CL_COMMAND_READ_IMAGE",
                                             "CL_COMMAND_WRITE_IMAGE",
                                             "CL_COMMAND_COPY_IMAGE",
                                             "CL_COMMAND_COPY_IMAGE_TO_BUFFER",
                                             "CL_COMMAND_COPY_BUFFER_TO_IMAGE",
                                             "CL_COMMAND_MAP_BUFFER",
                                             "CL_COMMAND_MAP_IMAGE",
                                             "CL_COMMAND_UNMAP_MEM_OBJECT",
                                             "CL_COMMAND_MARKER",
                                             "CL_COMMAND_ACQUIRE_GL_OBJECTS",
                                             "CL_COMMAND_RELEASE_GL_OBJECTS",
                                             "CL_COMMAND_READ_BUFFER_RECT",
                                             "CL_COMMAND_WRITE_BUFFER_RECT",
                                             "CL_COMMAND_COPY_BUFFER_RECT",
                                             "CL_COMMAND_USER",
                                             "CL_COMMAND_BARRIER",
                                             "CL_COMMAND_MIGRATE_MEM_OBJECTS",
                                             "CL_COMMAND_FILL_BUFFER",
                                             "CL_COMMAND_FILL_IMAGE",
                                             "CL_COMMAND_SVM_FREE",
                                             "CL_COMMAND_SVM_MEMCPY",
                                             "CL_COMMAND_SVM_MEMFILL",
                                             "CL_COMMAND_SVM_MAP",
                                             "CL_COMMAND_SVM_UNMAP",
                                             "CL_COMMAND_SVM_MIGRATE_MEM"}};

    for (int i = CL_COMMAND_NDRANGE_KERNEL; i <= CL_COMMAND_SVM_MIGRATE_MEM; i++) {
        EXPECT_STREQ(expected[i - CL_COMMAND_NDRANGE_KERNEL].c_str(), NEO::cmdTypetoString(i).c_str());
    }

    std::stringstream stream;
    stream << "CMD_UNKNOWN:" << (cl_command_type)-1;

    EXPECT_STREQ(stream.str().c_str(), NEO::cmdTypetoString(-1).c_str());

    EXPECT_STREQ("CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR", NEO::cmdTypetoString(CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR).c_str());
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenIsLinearStoragePreferredThenReturnValidValue) {
    bool tilingSupported = defaultHwInfo->capabilityTable.supportsImages;

    const uint32_t numImageTypes = 6;
    const cl_mem_object_type imgTypes[numImageTypes] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE1D_BUFFER,
                                                        CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D};
    cl_image_desc imgDesc = {};
    MockContext context;
    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, 0, 1, nullptr, retVal));

    auto &productHelper = getHelper<ProductHelper>();

    for (uint32_t i = 0; i < numImageTypes; i++) {
        imgDesc.image_type = imgTypes[i];
        imgDesc.buffer = nullptr;

        bool allowedType = imgTypes[i] == (CL_MEM_OBJECT_IMAGE2D) || (imgTypes[i] == CL_MEM_OBJECT_IMAGE3D) ||
                           (imgTypes[i] == CL_MEM_OBJECT_IMAGE2D_ARRAY);

        // dont force linear storage
        EXPECT_EQ((tilingSupported & allowedType), !productHelper.isLinearStoragePreferred(Image::isImage1d(imgDesc), false));
        {
            DebugManagerStateRestore restore;
            debugManager.flags.ForceLinearImages.set(true);
            // dont force linear storage + debug flag
            EXPECT_TRUE(productHelper.isLinearStoragePreferred(Image::isImage1d(imgDesc), false));
        }
        // force linear storage
        EXPECT_TRUE(productHelper.isLinearStoragePreferred(Image::isImage1d(imgDesc), true));

        // dont force linear storage + create from buffer
        imgDesc.buffer = buffer.get();
        EXPECT_TRUE(productHelper.isLinearStoragePreferred(Image::isImage1d(imgDesc), false));
    }
}

TEST(ClGfxCoreHelperTestCreate, WhenClGfxCoreHelperIsCalledWithUnknownGfxCoreThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, ClGfxCoreHelper::create(IGFX_UNKNOWN_CORE));
}

using ClGfxCoreHelperTest = Test<ClDeviceFixture>;
HWTEST_F(ClGfxCoreHelperTest, givenKernelInfoWhenCheckingRequiresAuxResolvesThenCorrectValuesAreReturned) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    KernelInfo kernelInfo{};

    ArgDescriptor argDescriptorValue(ArgDescriptor::ArgType::argTValue);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorValue);
    EXPECT_FALSE(clGfxCoreHelper.requiresAuxResolves(kernelInfo));

    ArgDescriptor argDescriptorPointer(ArgDescriptor::ArgType::argTPointer);
    argDescriptorPointer.as<ArgDescPointer>().accessedUsingStatelessAddressingMode = true;
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorPointer);
    EXPECT_TRUE(clGfxCoreHelper.requiresAuxResolves(kernelInfo));
}

TEST_F(ClGfxCoreHelperTest, givenGenHelperWhenKernelArgumentIsNotPureStatefulThenRequireNonAuxMode) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto isPureStateful : {false, true}) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;

        EXPECT_EQ(!argAsPtr.isPureStateful(), clGfxCoreHelper.requiresNonAuxMode(argAsPtr));
    }
}

HWTEST_F(ClGfxCoreHelperTest, WhenCheckingIsLimitationForPreemptionNeededThenReturnFalse) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    EXPECT_FALSE(clGfxCoreHelper.isLimitationForPreemptionNeeded());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, ClGfxCoreHelperTest, givenCLImageFormatsWhenCallingIsFormatRedescribableThenCorrectValueReturned) {
    static const cl_image_format redescribeFormats[] = {
        {CL_R, CL_UNSIGNED_INT8},
        {CL_R, CL_UNSIGNED_INT16},
        {CL_R, CL_UNSIGNED_INT32},
        {CL_RG, CL_UNSIGNED_INT32},
        {CL_RGBA, CL_UNSIGNED_INT32},
    };

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    auto formats = SurfaceFormats::readWrite();
    for (const auto &format : formats) {
        const cl_image_format oclFormat = format.oclImageFormat;
        bool expectedResult = true;
        for (const auto &nonRedescribableFormat : redescribeFormats) {
            expectedResult &= (memcmp(&oclFormat, &nonRedescribableFormat, sizeof(cl_image_format)) != 0);
        }
        EXPECT_EQ(expectedResult, clGfxCoreHelper.isFormatRedescribable(oclFormat));
    }
}
