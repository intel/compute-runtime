/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device_get_cap.inl"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"

using namespace NEO;

class Nv12ImageTest : public testing::Test {
  public:
    void computeExpectedOffsets(Image *image) {
        SurfaceOffsets expectedSurfaceOffsets = {0};
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        SurfaceOffsets requestedOffsets = {0};

        auto mockResInfo = static_cast<MockGmmResourceInfo *>(image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo.get());
        mockResInfo->getOffset(reqOffsetInfo);

        if (image->getImageDesc().mem_object) {
            expectedSurfaceOffsets.offset = reqOffsetInfo.Render.Offset;
            expectedSurfaceOffsets.xOffset = reqOffsetInfo.Render.XOffset / (mockResInfo->getBitsPerPixel() / 8);
            expectedSurfaceOffsets.yOffset = reqOffsetInfo.Render.YOffset;
        }
        expectedSurfaceOffsets.yOffsetForUVplane = reqOffsetInfo.Lock.Offset / reqOffsetInfo.Lock.Pitch;

        image->getSurfaceOffsets(requestedOffsets);

        EXPECT_EQ(expectedSurfaceOffsets.offset, requestedOffsets.offset);
        EXPECT_EQ(expectedSurfaceOffsets.xOffset, requestedOffsets.xOffset);
        EXPECT_EQ(expectedSurfaceOffsets.yOffset, requestedOffsets.yOffset);
        EXPECT_EQ(expectedSurfaceOffsets.yOffsetForUVplane, requestedOffsets.yOffsetForUVplane);
    }

  protected:
    void SetUp() override {
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_NV12_INTEL;

        imageDesc.mem_object = NULL;
        imageDesc.image_array_size = 0;
        imageDesc.image_depth = 1;
        imageDesc.image_height = 4 * 4; // Valid values multiple of 4
        imageDesc.image_width = 4 * 4;  // Valid values multiple of 4

        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;

        flags = CL_MEM_HOST_NO_ACCESS;
    }

    void validateImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                                 surfaceFormat, &imageDesc, nullptr);
    }

    Image *createImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        return Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             flags, 0, surfaceFormat, &imageDesc, nullptr, retVal);
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_mem_flags flags;
};

TEST_F(Nv12ImageTest, WhenImageIsCreatedThenIsNv12ImageIsTrue) {
    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(isNV12Image(&image->getImageFormat()));
}

TEST_F(Nv12ImageTest, GivenValidImageWhenValidatingThenSuccessIsReturned) {
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, GivenSnormInt16ImageTypeWhenValidatingThenImageFormatNotSupportedErrorIsReturned) {
    imageFormat.image_channel_data_type = CL_SNORM_INT16;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal);
}

TEST_F(Nv12ImageTest, Given1dImageTypeWhenValidatingThenImageFormatNotSupportedErrorIsReturned) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, GivenInvalidImageHeightWhenValidatingThenInvalidImageDescriptorErrorIsReturned) {
    imageDesc.image_height = 17;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, GivenInvalidImageWidthWhenValidatingThenInvalidImageDescriptorErrorIsReturned) {
    imageDesc.image_width = 17;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, GivenInvalidImageFlagWhenValidatingThenInvalidValueErrorIsReturned) {
    flags &= ~(CL_MEM_HOST_NO_ACCESS);
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(Nv12ImageTest, GivenYPlaneWhenValidatingThenSuccessIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(&context);

    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image.get();
    imageDesc.image_depth = 0; // Plane Y of NV12 image

    validateImageWithFlags(CL_MEM_READ_WRITE);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, GivenUVPlaneWhenValidatingThenSuccessIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(&context);

    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image.get();
    imageDesc.image_depth = 1; // Plane UV of NV12 image

    validateImageWithFlags(CL_MEM_READ_WRITE);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, GivenInvalidImageDepthWhenValidatingThenInvalidImageDescriptorErrorIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);
    imageDesc.mem_object = image.get();
    imageDesc.image_depth = 3; // Invalid Plane of NV12 image

    validateImageWithFlags(CL_MEM_READ_WRITE);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, given2DImageWhenPassedToValidateImageTraitsThenValidateReturnsSuccess) {
    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image.get();
    imageDesc.image_depth = 0;

    retVal = Image::validateImageTraits(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        &imageFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, given1DImageWhenPassedAsParentImageThenValidateImageTraitsReturnsSuccess) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image.get();
    imageDesc.image_depth = 0;

    retVal = Image::validateImageTraits(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        &imageFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, givenBufferWhenPassedAsNV12ParentImageThenValidateImageTraitsReturnsInvalidDesriptor) {
    MockBuffer buffer;

    imageDesc.mem_object = &buffer;
    imageDesc.image_depth = 0; // Plane of NV12 image

    retVal = Image::validateImageTraits(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        &imageFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, WhenImageIsCreatedThenOffsetsAreZero) {
    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    auto rowPitch = image->getHostPtrRowPitch();
    EXPECT_NE(0u, rowPitch);
    SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);
    EXPECT_EQ(0u, surfaceOffsets.offset);
    EXPECT_EQ(0u, surfaceOffsets.xOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffset);
    EXPECT_NE(0u, surfaceOffsets.yOffsetForUVplane);
}

TEST_F(Nv12ImageTest, WhenCreatingYPlaneImageThenDimensionsAreSetCorrectly) {
    // Create Parent NV12 image
    std::unique_ptr<Image> imageNV12{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12.get();
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;
    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;

    // Create NV12 Y Plane image
    std::unique_ptr<Image> imageYPlane{createImageWithFlags(CL_MEM_READ_WRITE)};
    ASSERT_NE(nullptr, imageYPlane);
    EXPECT_EQ(true, imageYPlane->isImageFromImage());
    EXPECT_EQ(imageNV12->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()),
              imageYPlane->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(GMM_PLANE_Y, imageYPlane->getPlane());

    cl_image_desc parentDimensions, planeDimensions;
    parentDimensions = imageNV12->getImageDesc();
    planeDimensions = imageYPlane->getImageDesc();

    EXPECT_EQ(parentDimensions.image_height, planeDimensions.image_height);
    EXPECT_EQ(parentDimensions.image_width, planeDimensions.image_width);
    EXPECT_EQ(0u, planeDimensions.image_depth);
    EXPECT_NE(0u, planeDimensions.image_row_pitch);
    EXPECT_EQ(parentDimensions.image_slice_pitch, planeDimensions.image_slice_pitch);
    EXPECT_EQ(parentDimensions.image_type, planeDimensions.image_type);
    EXPECT_EQ(parentDimensions.image_array_size, planeDimensions.image_array_size);

    computeExpectedOffsets(imageYPlane.get());
    computeExpectedOffsets(imageNV12.get());
}

TEST_F(Nv12ImageTest, WhenCreatingUVPlaneImageThenDimensionsAreSetCorrectly) {
    // Create Parent NV12 image
    std::unique_ptr<Image> imageNV12{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12.get();
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;
    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageUVPlane{createImageWithFlags(CL_MEM_READ_WRITE)};
    ASSERT_NE(nullptr, imageUVPlane);
    EXPECT_EQ(true, imageUVPlane->isImageFromImage());
    EXPECT_EQ(imageNV12->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()),
              imageUVPlane->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(GMM_PLANE_U, imageUVPlane->getPlane());

    cl_image_desc parentDimensions, planeDimensions;
    parentDimensions = imageNV12->getImageDesc();
    planeDimensions = imageUVPlane->getImageDesc();

    EXPECT_EQ(parentDimensions.image_height / 2, planeDimensions.image_height);
    EXPECT_EQ(parentDimensions.image_width / 2, planeDimensions.image_width);
    EXPECT_EQ(0u, planeDimensions.image_depth);
    EXPECT_EQ(parentDimensions.image_row_pitch, planeDimensions.image_row_pitch);
    EXPECT_NE(0u, planeDimensions.image_row_pitch);

    EXPECT_EQ(parentDimensions.image_slice_pitch, planeDimensions.image_slice_pitch);
    EXPECT_EQ(parentDimensions.image_type, planeDimensions.image_type);
    EXPECT_EQ(parentDimensions.image_array_size, planeDimensions.image_array_size);

    computeExpectedOffsets(imageUVPlane.get());
    computeExpectedOffsets(imageNV12.get());
}

TEST_F(Nv12ImageTest, GivenOffsetOfUVPlaneWhenCreatingUVPlaneImageThenDimensionsAreSetCorrectly) {
    // This size returns offset of UV plane, and 0 yOffset
    imageDesc.image_height = 64; // Valid values multiple of 4
    imageDesc.image_width = 64;  // Valid values multiple of 4

    // Create Parent NV12 image
    std::unique_ptr<Image> imageNV12{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12.get();
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;
    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageUVPlane{createImageWithFlags(CL_MEM_READ_WRITE)};

    ASSERT_NE(nullptr, imageUVPlane);
    EXPECT_EQ(true, imageUVPlane->isImageFromImage());
    EXPECT_EQ(imageNV12->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()),
              imageUVPlane->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));

    cl_image_desc parentDimensions, planeDimensions;
    parentDimensions = imageNV12->getImageDesc();
    planeDimensions = imageUVPlane->getImageDesc();

    EXPECT_EQ(parentDimensions.image_height / 2, planeDimensions.image_height);
    EXPECT_EQ(parentDimensions.image_width / 2, planeDimensions.image_width);
    EXPECT_EQ(0u, planeDimensions.image_depth);
    EXPECT_EQ(parentDimensions.image_row_pitch, planeDimensions.image_row_pitch);
    EXPECT_NE(0u, planeDimensions.image_row_pitch);
    EXPECT_EQ(parentDimensions.image_slice_pitch, planeDimensions.image_slice_pitch);
    EXPECT_EQ(parentDimensions.image_type, planeDimensions.image_type);
    EXPECT_EQ(parentDimensions.image_array_size, planeDimensions.image_array_size);

    computeExpectedOffsets(imageUVPlane.get());
    computeExpectedOffsets(imageNV12.get());
}

HWTEST_F(Nv12ImageTest, WhenCreatingParentImageThenPlanesAreWritten) {
    KernelBinaryHelper kbHelper(KernelBinaryHelper::BUILT_INS_WITH_IMAGES);
    auto device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    char hostPtr[16 * 16 * 16];
    auto contextWithMockCmdQ = new MockContext(device.get(), true);
    auto cmdQ = new MockCommandQueueHw<FamilyType>(contextWithMockCmdQ, device.get(), 0);
    contextWithMockCmdQ->overrideSpecialQueueAndDecrementRefCount(cmdQ, device->getRootDeviceIndex());

    // Create Parent NV12 image
    cl_mem_flags flags = CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> imageNV12{Image::create(contextWithMockCmdQ,
                                                   ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                                                   flags, 0, surfaceFormat, &imageDesc, hostPtr, retVal)};

    EXPECT_EQ(imageNV12->isTiledAllocation() ? 2u : 0u, cmdQ->enqueueWriteImageCounter);
    ASSERT_NE(nullptr, imageNV12);
    contextWithMockCmdQ->release();
}

HWTEST_F(Nv12ImageTest, WhenSettingImageArgThenSurfaceStateIsCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState;

    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);
    image->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());

    EXPECT_EQ(surfaceOffsets.xOffset, surfaceState.getXOffset());
    EXPECT_EQ(surfaceOffsets.yOffset, surfaceState.getYOffset());
    EXPECT_EQ(surfaceOffsets.yOffsetForUVplane, surfaceState.getYOffsetForUOrUvPlane());

    // NV 12 image has correct alpha channel == one
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE, surfaceState.getShaderChannelSelectAlpha());
}

HWTEST_F(Nv12ImageTest, givenNv12ImageArrayAndImageArraySizeIsZeroWhenCallingSetImageArgThenDoNotProgramSurfaceArray) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState;

    cl_image_desc imageDesc = Image2dDefaults::imageDesc;
    imageDesc.image_array_size = 1;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    cl_image_format imageFormat = Image2dDefaults::imageFormat;
    imageFormat.image_channel_order = CL_NV12_INTEL;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    std::unique_ptr<Image> image{Image2dHelperUlt<>::create(&context, &imageDesc, &imageFormat)};
    image->setCubeFaceIndex(__GMM_NO_CUBE_MAP);

    image->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());

    if (ImageSurfaceStateHelper<FamilyType>::imageAsArrayWithArraySizeOf1NotPreferred()) {
        EXPECT_FALSE(surfaceState.getSurfaceArray());
    } else {
        EXPECT_TRUE(surfaceState.getSurfaceArray());
    }
}

HWTEST_F(Nv12ImageTest, WhenSettingImageArgUvPlaneImageThenOffsetSurfaceBaseAddressAndCorrectTileModeAreSet) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState;
    std::unique_ptr<Image> imageNV12{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12.get();
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;
    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageUVPlane{createImageWithFlags(CL_MEM_READ_WRITE)};

    ASSERT_NE(nullptr, imageUVPlane);
    EXPECT_EQ(imageNV12->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()),
              imageUVPlane->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));

    SurfaceOffsets surfaceOffsets;
    imageUVPlane->getSurfaceOffsets(surfaceOffsets);
    imageUVPlane->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());

    EXPECT_EQ(imageUVPlane->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getGpuAddress() + surfaceOffsets.offset, surfaceState.getSurfaceBaseAddress());

    auto tileMode = RENDER_SURFACE_STATE::TILE_MODE_LINEAR;
    if (imageNV12->isTiledAllocation()) {
        tileMode = static_cast<typename RENDER_SURFACE_STATE::TILE_MODE>(MockGmmResourceInfo::yMajorTileModeValue);
    }

    EXPECT_EQ(tileMode, surfaceState.getTileMode());
}

HWTEST_F(Nv12ImageTest, WhenSettingMediaImageArgThenSurfaceStateIsCorrect) {
    using MEDIA_SURFACE_STATE = typename FamilyType::MEDIA_SURFACE_STATE;
    MEDIA_SURFACE_STATE surfaceState;

    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);
    image->setMediaImageArg(&surfaceState, context.getDevice(0)->getRootDeviceIndex());

    EXPECT_EQ(surfaceOffsets.xOffset, surfaceState.getXOffsetForUCb());
    EXPECT_EQ(surfaceOffsets.yOffset, surfaceState.getXOffsetForUCb());
    EXPECT_EQ(surfaceOffsets.yOffsetForUVplane, surfaceState.getYOffsetForUCb());
    EXPECT_EQ(image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getGpuAddress() + surfaceOffsets.offset,
              surfaceState.getSurfaceBaseAddress());
}

TEST_F(Nv12ImageTest, WhenRedescribingThenNV12ImageAndUVPlaneImageHaveCorrectOffsets) {
    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};
    ASSERT_NE(nullptr, image);

    std::unique_ptr<Image> imageRedescribed{image->redescribe()};
    ASSERT_NE(nullptr, imageRedescribed);

    SurfaceOffsets imageOffsets, redescribedOffsets;
    image->getSurfaceOffsets(imageOffsets);
    imageRedescribed->getSurfaceOffsets(redescribedOffsets);

    EXPECT_EQ(imageOffsets.xOffset, redescribedOffsets.xOffset);
    EXPECT_EQ(imageOffsets.yOffset, redescribedOffsets.yOffset);
    EXPECT_EQ(imageOffsets.yOffsetForUVplane, redescribedOffsets.yOffsetForUVplane);

    imageDesc.mem_object = image.get();
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;
    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageUVPlane{createImageWithFlags(CL_MEM_READ_WRITE)};
    ASSERT_NE(nullptr, imageUVPlane);

    imageRedescribed.reset(imageUVPlane->redescribe());
    ASSERT_NE(nullptr, imageRedescribed);

    imageUVPlane->getSurfaceOffsets(imageOffsets);
    imageRedescribed->getSurfaceOffsets(redescribedOffsets);

    EXPECT_EQ(imageOffsets.xOffset, redescribedOffsets.xOffset);
    EXPECT_EQ(imageOffsets.yOffset, redescribedOffsets.yOffset);
    EXPECT_EQ(imageOffsets.yOffsetForUVplane, redescribedOffsets.yOffsetForUVplane);
}

TEST_F(Nv12ImageTest, GivenInvalidImageHeightWhenValidatingPlanarYuvThenInvalidImageSizeErrorIsReturned) {

    auto pClDevice = context.getDevice(0);
    const size_t *maxHeight = nullptr;
    size_t srcSize = 0;
    size_t retSize = 0;

    ASSERT_NE(nullptr, pClDevice);

    pClDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL>(reinterpret_cast<const void *&>(maxHeight), srcSize, retSize);

    imageDesc.image_height = *maxHeight + 12;
    retVal = Image::validatePlanarYUV(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &pClDevice->getDevice()),
                                      &imageDesc, nullptr);

    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_F(Nv12ImageTest, GivenInvalidImageWidthWhenValidatingPlanarYuvThenInvalidImageSizeErrorIsReturned) {

    auto pClDevice = context.getDevice(0);
    const size_t *maxWidth = nullptr;
    size_t srcSize = 0;
    size_t retSize = 0;

    ASSERT_NE(nullptr, pClDevice);

    pClDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL>(reinterpret_cast<const void *&>(maxWidth), srcSize, retSize);

    imageDesc.image_width = *maxWidth + 12;
    retVal = Image::validatePlanarYUV(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &pClDevice->getDevice()),
                                      &imageDesc, nullptr);

    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_F(Nv12ImageTest, GivenValidImageHeightWhenValidatingPlanarYuvThenSuccessIsReturned) {
    retVal = Image::validatePlanarYUV(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                                      &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, GivenValidImageWidthWhenValidatingPlanarYuvThenSuccessIsReturned) {
    retVal = Image::validatePlanarYUV(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                                      &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
