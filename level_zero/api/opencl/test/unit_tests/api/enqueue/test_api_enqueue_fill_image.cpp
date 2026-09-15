/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/header/common_matchers.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "CL/cl.h"

#include <array>
#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct FillImageRecordingKernel : public L0::ult::Mock<::L0::KernelImp> {
    ze_result_t setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal, bool isPacked, uint32_t mipLevel) override {
        redescribedImageMipLevel = mipLevel;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) override {
        if (argIndex == dstOffsetArgIndex && argSize == sizeof(dstOffset)) {
            memcpy(dstOffset.data(), pArgValue, sizeof(dstOffset));
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY, uint32_t globalSizeZ,
                                 uint32_t *groupSizeX, uint32_t *groupSizeY, uint32_t *groupSizeZ) override {
        *groupSizeX = 1u;
        *groupSizeY = 1u;
        *groupSizeZ = 1u;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) override {
        return ZE_RESULT_SUCCESS;
    }

    static constexpr uint32_t dstOffsetArgIndex = 2u;

    std::array<uint32_t, 4> dstOffset{};
    uint32_t redescribedImageMipLevel = std::numeric_limits<uint32_t>::max();
};

struct FillImageBuiltInKernelLib : public L0::ult::MockBuiltInKernelLibImpl {
    using MockBuiltInKernelLibImpl::MockBuiltInKernelLibImpl;

    ::L0::Kernel *getImageFunction(::L0::ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override {
        return &recordingKernel;
    }

    FillImageRecordingKernel recordingKernel{};
};

struct LeoEnqueueFillImageTest : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();

        if (!neoDevice->getHardwareInfo().capabilityTable.supportsImages) {
            GTEST_SKIP();
        }

        auto l0Device = clDevice->getL0Object();
        auto builtInKernelLib = std::make_unique<FillImageBuiltInKernelLib>(l0Device, neoDevice->getBuiltIns());
        recordingKernel = &builtInKernelLib->recordingKernel;
        l0Device->builtins = std::move(builtInKernelLib);
    }

    cl_mem createImage2d(cl_uint numMipLevels) {
        cl_image_format imageFormat{CL_RGBA, CL_UNSIGNED_INT8};
        cl_image_desc imageDesc{};
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = imageWidth;
        imageDesc.image_height = imageHeight;
        imageDesc.num_mip_levels = numMipLevels;

        return clCreateImage(clContext, CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &createImageResult);
    }

    cl_int createImageResult = CL_SUCCESS;
    static constexpr size_t imageWidth = 16u;
    static constexpr size_t imageHeight = 16u;

    FillImageRecordingKernel *recordingKernel = nullptr;
};

HWTEST2_F(LeoEnqueueFillImageTest, givenMipMappedImageWhenEnqueueFillImageThenMipLevelIsPassedToRedescribedImageArgumentAndNotToDstOffset, ImageSupport) {
    constexpr cl_uint numMipLevels = 4u;
    constexpr size_t mipLevel = 2u;

    auto image = createImage2d(numMipLevels);
    ASSERT_EQ(CL_SUCCESS, createImageResult);
    ASSERT_NE(nullptr, image);

    const uint32_t fillColor[4] = {1u, 2u, 3u, 4u};
    const size_t origin[3] = {1u, 1u, mipLevel};
    const size_t region[3] = {4u, 4u, 1u};

    auto retVal = clEnqueueFillImage(getCommandQueue(), image, fillColor, origin, region, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<uint32_t>(mipLevel), recordingKernel->redescribedImageMipLevel);

    const std::array<uint32_t, 4> expectedDstOffset{1u, 1u, 0u, 0u};
    EXPECT_EQ(expectedDstOffset, recordingKernel->dstOffset);

    clReleaseMemObject(image);
}

HWTEST2_F(LeoEnqueueFillImageTest, givenImageWithoutMipMapsWhenEnqueueFillImageThenZeroMipLevelIsPassedToRedescribedImageArgument, ImageSupport) {
    auto image = createImage2d(0u);
    ASSERT_EQ(CL_SUCCESS, createImageResult);
    ASSERT_NE(nullptr, image);

    const uint32_t fillColor[4] = {1u, 2u, 3u, 4u};
    const size_t origin[3] = {2u, 3u, 0u};
    const size_t region[3] = {4u, 4u, 1u};

    auto retVal = clEnqueueFillImage(getCommandQueue(), image, fillColor, origin, region, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, recordingKernel->redescribedImageMipLevel);

    const std::array<uint32_t, 4> expectedDstOffset{2u, 3u, 0u, 0u};
    EXPECT_EQ(expectedDstOffset, recordingKernel->dstOffset);

    clReleaseMemObject(image);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
