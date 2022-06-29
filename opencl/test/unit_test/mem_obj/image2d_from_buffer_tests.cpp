/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/raii_hw_helper.h"

#include "opencl/source/cl_device/cl_device_info_map.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

namespace NEO {
extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];
}

// Tests for cl_khr_image2d_from_buffer
class Image2dFromBufferTest : public ::testing::Test {
  public:
    Image2dFromBufferTest() {}

  protected:
    void SetUp() override {
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_RGBA;

        imageDesc.image_array_size = 0;
        imageDesc.image_depth = 0;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_height = 128;
        imageDesc.image_width = 256;
        imageDesc.num_mip_levels = 0;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_samples = 0;

        size = 128 * 256 * 4;
        hostPtr = alignedMalloc(size, 16);
        ASSERT_NE(nullptr, hostPtr);
        imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_USE_HOST_PTR, size, hostPtr, &retVal);
        ASSERT_NE(nullptr, imageDesc.mem_object);
    }
    void TearDown() override {
        clReleaseMemObject(imageDesc.mem_object);
        alignedFree(hostPtr);
    }

    Image *createImage() {
        cl_mem_flags flags = CL_MEM_READ_ONLY;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        return Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             flags, 0, surfaceFormat, &imageDesc, NULL, retVal);
    }
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    void *hostPtr;
    size_t size;
};

TEST_F(Image2dFromBufferTest, WhenCreatingImage2dFromBufferThenImagePropertiesAreCorrect) {
    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(1, buffer->getRefInternalCount());

    auto imageFromBuffer = createImage();
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, buffer->getRefInternalCount());
    EXPECT_NE(nullptr, imageFromBuffer);

    EXPECT_FALSE(imageFromBuffer->isTiledAllocation());
    EXPECT_EQ(imageFromBuffer->getCubeFaceIndex(), static_cast<uint32_t>(__GMM_NO_CUBE_MAP));

    delete imageFromBuffer;
    EXPECT_EQ(1, buffer->getRefInternalCount());
}

TEST_F(Image2dFromBufferTest, givenBufferWhenCreateImage2dArrayFromBufferThenImageDescriptorIsInvalid) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}
TEST_F(Image2dFromBufferTest, WhenCreatingImageThenRowPitchIsCorrect) {
    auto imageFromBuffer = createImage();
    ASSERT_NE(nullptr, imageFromBuffer);
    EXPECT_EQ(1024u, imageFromBuffer->getImageDesc().image_row_pitch);
    delete imageFromBuffer;
}
TEST_F(Image2dFromBufferTest, givenInvalidRowPitchWhenCreateImage2dFromBufferThenReturnsError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    char ptr[10];
    imageDesc.image_row_pitch = 255;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, ptr);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenRowPitchThatIsGreaterThenComputedWhenImageIsCreatedThenPassedRowPitchIsUsedInsteadOfComputed) {
    auto computedSize = imageDesc.image_width * 4;
    auto passedSize = computedSize * 2;
    imageDesc.image_row_pitch = passedSize;

    auto imageFromBuffer = createImage();

    EXPECT_EQ(passedSize, imageFromBuffer->getHostPtrRowPitch());
    delete imageFromBuffer;
}

TEST_F(Image2dFromBufferTest, GivenInvalidHostPtrAlignmentWhenCreatingImageThenInvalidImageFormatDescriptorErrorIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    std::unique_ptr<void, decltype(free) *> myHostPtr(malloc(size + 1), free);
    ASSERT_NE(nullptr, myHostPtr);
    void *nonAlignedHostPtr = myHostPtr.get();
    if ((reinterpret_cast<uint64_t>(myHostPtr.get()) % 4) == 0) {
        nonAlignedHostPtr = reinterpret_cast<void *>((reinterpret_cast<uint64_t>(myHostPtr.get()) + 1));
    }

    cl_mem origBuffer = imageDesc.mem_object;
    imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_USE_HOST_PTR, size, nonAlignedHostPtr, &retVal);
    ASSERT_NE(nullptr, imageDesc.mem_object);
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);

    clReleaseMemObject(imageDesc.mem_object);
    imageDesc.mem_object = origBuffer;
}

TEST_F(Image2dFromBufferTest, givenInvalidFlagsWhenValidateIsCalledThenReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    cl_mem_flags flags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_COPY_HOST_PTR};

    for (auto flag : flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flag, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flag, 0, 0, &context.getDevice(0)->getDevice()),
                                 surfaceFormat, &imageDesc, reinterpret_cast<void *>(0x12345));
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
    }
}

TEST_F(Image2dFromBufferTest, givenOneChannel8BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenOneChannel16BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width / 2;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT16;
    imageFormat.image_channel_order = CL_R;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel8BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width / 4;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    const auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel16BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width / 8;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT16;
    imageFormat.image_channel_order = CL_RGBA;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel8BitColorsAndNotTooLargeRowPitchSpecifiedWhenValidatingSurfaceFormatThenDoNotReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    imageDesc.image_height = castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    imageDesc.image_row_pitch = imageDesc.image_width;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel8BitColorsAndTooLargeRowPitchSpecifiedWhenValidatingSurfaceFormatThenReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    const auto pitchAlignment = &ClDeviceInfoTable::Map<CL_DEVICE_IMAGE_PITCH_ALIGNMENT>::getValue(*context.getDevice(0u));
    imageDesc.image_height = castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    imageDesc.image_row_pitch = imageDesc.image_width + *pitchAlignment;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenUnalignedImageWidthAndNoSpaceInBufferForAlignmentWhenValidatingSurfaceFormatThenReturnError) {
    REQUIRE_IMAGES_OR_SKIP(&context);
    static_cast<MockClDevice *>(context.getDevice(0))->deviceInfo.imagePitchAlignment = 128;
    imageDesc.image_width = 64;
    imageDesc.image_height = castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, GivenPlatformWhenGettingExtensionStringThenImage2dFromBufferExtensionIsCorrectlyReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto hwInfo = device->getHardwareInfo();
    const auto &caps = device->getDeviceInfo();
    std::string extensions = caps.deviceExtensions;
    size_t found = extensions.find("cl_khr_image2d_from_buffer");
    if (hwInfo.capabilityTable.supportsImages) {
        EXPECT_NE(std::string::npos, found);
    } else {
        EXPECT_EQ(std::string::npos, found);
    }
}

TEST_F(Image2dFromBufferTest, WhenCreatingImageThenHostPtrIsCorrectlySet) {
    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(1, buffer->getRefInternalCount());

    auto imageFromBuffer = createImage();
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(buffer->getHostPtr(), imageFromBuffer->getHostPtr());
    EXPECT_EQ(true, imageFromBuffer->isMemObjZeroCopy());

    delete imageFromBuffer;
}

TEST_F(Image2dFromBufferTest, givenImageFromBufferWhenItIsRedescribedThenItReturnsProperImageFromBufferValue) {
    std::unique_ptr<Image> imageFromBuffer(createImage());
    EXPECT_TRUE(imageFromBuffer->isImageFromBuffer());
    std::unique_ptr<Image> redescribedImage(imageFromBuffer->redescribe());
    EXPECT_TRUE(redescribedImage->isImageFromBuffer());
    std::unique_ptr<Image> redescribedfillImage(imageFromBuffer->redescribeFillImage());
    EXPECT_TRUE(redescribedfillImage->isImageFromBuffer());
}

TEST_F(Image2dFromBufferTest, givenMemoryManagerSupporting1DImageFromBufferWhenNoBufferThenCreatesImage) {

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    auto storeMem = imageDesc.mem_object;
    imageDesc.mem_object = nullptr;

    std::unique_ptr<Image> imageFromBuffer(createImage());
    EXPECT_EQ(CL_SUCCESS, retVal);

    imageDesc.mem_object = storeMem;
}

TEST_F(Image2dFromBufferTest, givenBufferWhenImageFromBufferThenIsImageFromBufferSetAndAllocationTypeIsBuffer) {
    cl_int errCode = 0;
    auto buffer = Buffer::create(&context, 0, 1, nullptr, errCode);
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    auto memObj = imageDesc.mem_object;
    imageDesc.mem_object = buffer;

    std::unique_ptr<Image> imageFromBuffer(createImage());
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(imageFromBuffer->isImageFromBuffer());
    auto graphicsAllocation = imageFromBuffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    EXPECT_TRUE(AllocationType::BUFFER_HOST_MEMORY == graphicsAllocation->getAllocationType());

    buffer->release();
    imageDesc.mem_object = memObj;
}

HWTEST_F(Image2dFromBufferTest, givenBufferWhenImageFromBufferThenIsImageFromBufferSetAndAllocationTypeIsBufferNullptr) {
    class MockHwHelperHw : public HwHelperHw<FamilyType> {
      public:
        bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) override {
            return false;
        }
    };

    auto raiiFactory = RAIIHwHelperFactory<MockHwHelperHw>(context.getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);

    cl_int errCode = CL_SUCCESS;
    auto buffer = Buffer::create(&context, 0, 1, nullptr, errCode);
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    auto memObj = imageDesc.mem_object;
    imageDesc.mem_object = buffer;

    Image *imageFromBuffer = createImage();
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    EXPECT_EQ(imageFromBuffer, nullptr);

    buffer->release();
    imageDesc.mem_object = memObj;
}
