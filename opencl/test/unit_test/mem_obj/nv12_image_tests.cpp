/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/cl_device/cl_device_get_cap.inl"
#include "opencl/source/helpers/memory_properties_flags_helpers.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_gmm_resource_info.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

class Nv12ImageTest : public testing::Test {
  public:
    void computeExpectedOffsets(Image *image) {
        SurfaceOffsets expectedSurfaceOffsets = {0};
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        SurfaceOffsets requestedOffsets = {0};

        auto mockResInfo = reinterpret_cast<::testing::NiceMock<MockGmmResourceInfo> *>(image->getGraphicsAllocation()->getDefaultGmm()->gmmResourceInfo.get());
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
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
        retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), surfaceFormat, &imageDesc, nullptr);
    }

    Image *createImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
        return Image::create(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
                             flags, 0, surfaceFormat, &imageDesc, nullptr, retVal);
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_mem_flags flags;
};

TEST_F(Nv12ImageTest, isNV12ImageReturnsTrue) {
    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(IsNV12Image(&image->getImageFormat()));
    delete image;
}

TEST_F(Nv12ImageTest, validNV12ImageFormatAndDescriptor) {
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, invalidNV12ImageFormat) {
    imageFormat.image_channel_data_type = CL_SNORM_INT16;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal);
}

TEST_F(Nv12ImageTest, invalidNV12ImageType) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, DISABLED_invalidNV12ImageDepth) {
    imageDesc.image_depth = 2;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, invalidNV12ImageHeigth) {
    imageDesc.image_height = 17;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, invalidNV12ImageWidth) {
    imageDesc.image_width = 17;
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, invalidNV12ImageFlag) {
    flags &= ~(CL_MEM_HOST_NO_ACCESS);
    validateImageWithFlags(flags);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(Nv12ImageTest, validateNV12YPlane) {

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageDesc.image_depth = 0; // Plane Y of NV12 image

    validateImageWithFlags(CL_MEM_READ_WRITE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete image;
}

TEST_F(Nv12ImageTest, validateNV12YUVPlane) {

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageDesc.image_depth = 1; // Plane UV of NV12 image

    validateImageWithFlags(CL_MEM_READ_WRITE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete image;
}

TEST_F(Nv12ImageTest, givenNV12ImageWhenInvalidDepthIsPassedThenValidateFails) {

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageDesc.image_depth = 3; // Invalid Plane of NV12 image

    validateImageWithFlags(CL_MEM_READ_WRITE);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);

    delete image;
}

TEST_F(Nv12ImageTest, given2DImageWhenPassedToValidateImageTraitsThenValidateReturnsSuccess) {

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageDesc.image_depth = 0;

    retVal = Image::validateImageTraits(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_WRITE, 0, 0), &imageFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete image;
}

TEST_F(Nv12ImageTest, given1DImageWhenPassedAsParentImageThenValidateImageTraitsReturnsSuccess) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageDesc.image_depth = 0;

    retVal = Image::validateImageTraits(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_WRITE, 0, 0), &imageFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete image;
}

TEST_F(Nv12ImageTest, givenBufferWhenPassedAsNV12ParentImageThenValidateImageTraitsReturnsInvalidDesriptor) {
    MockBuffer Buffer;

    imageDesc.mem_object = &Buffer;
    imageDesc.image_depth = 0; // Plane of NV12 image

    retVal = Image::validateImageTraits(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_WRITE, 0, 0), &imageFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(Nv12ImageTest, createNV12Image) {

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    auto rowPitch = image->getHostPtrRowPitch();
    EXPECT_NE(0u, rowPitch);

    SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);

    EXPECT_EQ(0u, surfaceOffsets.offset);
    EXPECT_EQ(0u, surfaceOffsets.xOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffset);
    EXPECT_NE(0u, surfaceOffsets.yOffsetForUVplane);

    delete image;
}

TEST_F(Nv12ImageTest, createNV12YPlaneImage) {

    // Create Parent NV12 image
    auto imageNV12 = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12;

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;

    // Create NV12 Y Plane image
    auto imageYPlane = createImageWithFlags(CL_MEM_READ_WRITE);

    ASSERT_NE(nullptr, imageYPlane);
    EXPECT_EQ(true, imageYPlane->isImageFromImage());
    EXPECT_EQ(imageNV12->getGraphicsAllocation(), imageYPlane->getGraphicsAllocation());

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

    computeExpectedOffsets(imageYPlane);
    computeExpectedOffsets(imageNV12);

    delete imageYPlane;
    delete imageNV12;
}

TEST_F(Nv12ImageTest, createNV12UVPlaneImage) {
    // Create Parent NV12 image
    auto imageNV12 = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12;

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    auto imageUVPlane = createImageWithFlags(CL_MEM_READ_WRITE);

    ASSERT_NE(nullptr, imageUVPlane);

    EXPECT_EQ(true, imageUVPlane->isImageFromImage());
    EXPECT_EQ(imageNV12->getGraphicsAllocation(), imageUVPlane->getGraphicsAllocation());

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

    computeExpectedOffsets(imageUVPlane);
    computeExpectedOffsets(imageNV12);

    delete imageUVPlane;
    delete imageNV12;
}

TEST_F(Nv12ImageTest, createNV12UVPlaneImageWithOffsetOfUVPlane) {

    // This size returns offset of UV plane, and 0 yOffset
    imageDesc.image_height = 64; // Valid values multiple of 4
    imageDesc.image_width = 64;  // Valid values multiple of 4

    // Create Parent NV12 image
    auto imageNV12 = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12;

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    auto imageUVPlane = createImageWithFlags(CL_MEM_READ_WRITE);

    ASSERT_NE(nullptr, imageUVPlane);

    EXPECT_EQ(true, imageUVPlane->isImageFromImage());
    EXPECT_EQ(imageNV12->getGraphicsAllocation(), imageUVPlane->getGraphicsAllocation());

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

    computeExpectedOffsets(imageUVPlane);
    computeExpectedOffsets(imageNV12);

    delete imageUVPlane;
    delete imageNV12;
}

HWTEST_F(Nv12ImageTest, checkIfPlanesAreWritten) {
    KernelBinaryHelper kbHelper(KernelBinaryHelper::BUILT_INS);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    char hostPtr[16 * 16 * 16];

    auto contextWithMockCmdQ = new MockContext(device.get(), true);
    auto cmdQ = new MockCommandQueueHw<FamilyType>(contextWithMockCmdQ, device.get(), 0);

    contextWithMockCmdQ->overrideSpecialQueueAndDecrementRefCount(cmdQ);

    // Create Parent NV12 image
    cl_mem_flags flags = CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
    auto imageNV12 = Image::create(contextWithMockCmdQ, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
                                   flags, 0, surfaceFormat, &imageDesc, hostPtr, retVal);

    EXPECT_EQ(imageNV12->isTiledAllocation() ? 2u : 0u, cmdQ->EnqueueWriteImageCounter);

    ASSERT_NE(nullptr, imageNV12);
    contextWithMockCmdQ->release();
    delete imageNV12;
}

HWTEST_F(Nv12ImageTest, setImageArg) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE surfaceState;

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);
    image->setImageArg(&surfaceState, false, 0);

    EXPECT_EQ(surfaceOffsets.xOffset, surfaceState.getXOffset());
    EXPECT_EQ(surfaceOffsets.yOffset, surfaceState.getYOffset());
    EXPECT_EQ(surfaceOffsets.yOffsetForUVplane, surfaceState.getYOffsetForUOrUvPlane());

    // NV 12 image has correct alpha channel == one
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE, surfaceState.getShaderChannelSelectAlpha());

    delete image;
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
    std::unique_ptr<Image> image{Image2dHelper<>::create(&context, &imageDesc, &imageFormat)};
    image->setCubeFaceIndex(__GMM_NO_CUBE_MAP);

    image->setImageArg(&surfaceState, false, 0);
    EXPECT_FALSE(surfaceState.getSurfaceArray());
}

HWTEST_F(Nv12ImageTest, setImageArgUVPlaneImageSetsOffsetedSurfaceBaseAddressAndSetsCorrectTileMode) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE surfaceState;

    // Create Parent NV12 image
    auto imageNV12 = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, imageNV12);

    imageDesc.mem_object = imageNV12;

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    auto imageUVPlane = createImageWithFlags(CL_MEM_READ_WRITE);

    ASSERT_NE(nullptr, imageUVPlane);

    EXPECT_EQ(imageNV12->getGraphicsAllocation(), imageUVPlane->getGraphicsAllocation());

    SurfaceOffsets surfaceOffsets;
    imageUVPlane->getSurfaceOffsets(surfaceOffsets);

    imageUVPlane->setImageArg(&surfaceState, false, 0);

    EXPECT_EQ(imageUVPlane->getGraphicsAllocation()->getGpuAddress() + surfaceOffsets.offset, surfaceState.getSurfaceBaseAddress());

    auto tileMode = RENDER_SURFACE_STATE::TILE_MODE_LINEAR;
    if (imageNV12->isTiledAllocation()) {
        tileMode = RENDER_SURFACE_STATE::TILE_MODE_YMAJOR;
    }

    EXPECT_EQ(tileMode, surfaceState.getTileMode());

    delete imageUVPlane;
    delete imageNV12;
}

HWTEST_F(Nv12ImageTest, setMediaImageArg) {
    using MEDIA_SURFACE_STATE = typename FamilyType::MEDIA_SURFACE_STATE;

    MEDIA_SURFACE_STATE surfaceState;

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);
    image->setMediaImageArg(&surfaceState);

    EXPECT_EQ(surfaceOffsets.xOffset, surfaceState.getXOffsetForUCb());
    EXPECT_EQ(surfaceOffsets.yOffset, surfaceState.getXOffsetForUCb());
    EXPECT_EQ(surfaceOffsets.yOffsetForUVplane, surfaceState.getYOffsetForUCb());
    EXPECT_EQ(image->getGraphicsAllocation()->getGpuAddress() + surfaceOffsets.offset,
              surfaceState.getSurfaceBaseAddress());

    delete image;
}

TEST_F(Nv12ImageTest, redescribedNV12ImageAndUVPlaneImageHasCorrectOffsets) {

    auto image = createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    ASSERT_NE(nullptr, image);

    auto imageRedescribed = image->redescribe();

    ASSERT_NE(nullptr, imageRedescribed);

    SurfaceOffsets imageOffsets, redescribedOffsets;

    image->getSurfaceOffsets(imageOffsets);
    imageRedescribed->getSurfaceOffsets(redescribedOffsets);

    EXPECT_EQ(imageOffsets.xOffset, redescribedOffsets.xOffset);
    EXPECT_EQ(imageOffsets.yOffset, redescribedOffsets.yOffset);
    EXPECT_EQ(imageOffsets.yOffsetForUVplane, redescribedOffsets.yOffsetForUVplane);

    delete imageRedescribed;

    imageDesc.mem_object = image;

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane

    // Create NV12 UV Plane image
    auto imageUVPlane = createImageWithFlags(CL_MEM_READ_WRITE);

    ASSERT_NE(nullptr, imageUVPlane);

    imageRedescribed = imageUVPlane->redescribe();
    ASSERT_NE(nullptr, imageRedescribed);

    imageUVPlane->getSurfaceOffsets(imageOffsets);
    imageRedescribed->getSurfaceOffsets(redescribedOffsets);

    EXPECT_EQ(imageOffsets.xOffset, redescribedOffsets.xOffset);
    EXPECT_EQ(imageOffsets.yOffset, redescribedOffsets.yOffset);
    EXPECT_EQ(imageOffsets.yOffsetForUVplane, redescribedOffsets.yOffsetForUVplane);

    delete imageRedescribed;
    delete imageUVPlane;
    delete image;
}

TEST_F(Nv12ImageTest, invalidPlanarYUVImageHeight) {

    auto pDevice = context.getDevice(0);
    const size_t *maxHeight = nullptr;
    size_t srcSize = 0;
    size_t retSize = 0;

    ASSERT_NE(nullptr, pDevice);

    pDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL>(reinterpret_cast<const void *&>(maxHeight), srcSize, retSize);

    imageDesc.image_height = *maxHeight + 12;
    retVal = Image::validatePlanarYUV(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), &imageDesc, nullptr);

    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_F(Nv12ImageTest, invalidPlanarYUVImageWidth) {

    auto pDevice = context.getDevice(0);
    const size_t *maxWidth = nullptr;
    size_t srcSize = 0;
    size_t retSize = 0;

    ASSERT_NE(nullptr, pDevice);

    pDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL>(reinterpret_cast<const void *&>(maxWidth), srcSize, retSize);

    imageDesc.image_width = *maxWidth + 12;
    retVal = Image::validatePlanarYUV(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), &imageDesc, nullptr);

    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_F(Nv12ImageTest, validPlanarYUVImageHeight) {
    retVal = Image::validatePlanarYUV(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Nv12ImageTest, validPlanarYUVImageWidth) {
    retVal = Image::validatePlanarYUV(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
