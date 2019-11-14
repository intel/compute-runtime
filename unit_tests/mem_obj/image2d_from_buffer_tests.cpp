/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/raii_hw_helper.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"

using namespace NEO;

namespace NEO {
extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];
}

// Tests for cl_khr_image2d_from_buffer
class Image2dFromBufferTest : public DeviceFixture, public ::testing::Test {
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
        auto surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, &imageFormat);
        return Image::create(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), flags, 0, surfaceFormat, &imageDesc, NULL, retVal);
    }
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    void *hostPtr;
    size_t size;
};

TEST_F(Image2dFromBufferTest, CreateImage2dFromBuffer) {
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
    auto surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, &imageFormat);
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}
TEST_F(Image2dFromBufferTest, CalculateRowPitch) {
    auto imageFromBuffer = createImage();
    ASSERT_NE(nullptr, imageFromBuffer);
    EXPECT_NE(0u, imageFromBuffer->getImageDesc().image_row_pitch);
    EXPECT_EQ(1024u, imageFromBuffer->getImageDesc().image_row_pitch);
    delete imageFromBuffer;
}
TEST_F(Image2dFromBufferTest, givenInvalidRowPitchWhenCreateImage2dFromBufferThenReturnsError) {
    char ptr[10];
    imageDesc.image_row_pitch = 255;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, &imageFormat);
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, ptr);
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

TEST_F(Image2dFromBufferTest, InvalidHostPtrAlignment) {
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
    auto surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, &imageFormat);
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);

    clReleaseMemObject(imageDesc.mem_object);
    imageDesc.mem_object = origBuffer;
}

TEST_F(Image2dFromBufferTest, givenInvalidFlagsWhenValidateIsCalledThenReturnError) {
    cl_mem_flags flags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_COPY_HOST_PTR};

    for (auto flag : flags) {
        const auto surfaceFormat = Image::getSurfaceFormatFromTable(flag, &imageFormat);
        retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flag, 0), surfaceFormat, &imageDesc, reinterpret_cast<void *>(0x12345));
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
    }
}

TEST_F(Image2dFromBufferTest, givenOneChannel8BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    const auto surfaceFormat = static_cast<const SurfaceFormatInfo *>(Image::getSurfaceFormatFromTable(flags, &imageFormat));
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenOneChannel16BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width / 2;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT16;
    imageFormat.image_channel_order = CL_R;

    const auto surfaceFormat = static_cast<const SurfaceFormatInfo *>(Image::getSurfaceFormatFromTable(flags, &imageFormat));
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel8BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width / 4;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    const auto surfaceFormat = static_cast<const SurfaceFormatInfo *>(Image::getSurfaceFormatFromTable(flags, &imageFormat));
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel16BitColorsNoRowPitchSpecifiedAndTooLargeImageWhenValidatingSurfaceFormatThenReturnError) {
    imageDesc.image_height = 1 + castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width / 8;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT16;
    imageFormat.image_channel_order = CL_RGBA;

    const auto surfaceFormat = static_cast<const SurfaceFormatInfo *>(Image::getSurfaceFormatFromTable(flags, &imageFormat));
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel8BitColorsAndNotTooLargeRowPitchSpecifiedWhenValidatingSurfaceFormatThenDoNotReturnError) {
    imageDesc.image_height = castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    imageDesc.image_row_pitch = imageDesc.image_width;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    const auto surfaceFormat = static_cast<const SurfaceFormatInfo *>(Image::getSurfaceFormatFromTable(flags, &imageFormat));
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(Image2dFromBufferTest, givenFourChannel8BitColorsAndTooLargeRowPitchSpecifiedWhenValidatingSurfaceFormatThenReturnError) {
    const auto pitchAlignment = &DeviceInfoTable::Map<CL_DEVICE_IMAGE_PITCH_ALIGNMENT>::getValue(context.getDevice(0u)->getDeviceInfo());
    imageDesc.image_height = castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    imageDesc.image_row_pitch = imageDesc.image_width + *pitchAlignment;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    const auto surfaceFormat = static_cast<const SurfaceFormatInfo *>(Image::getSurfaceFormatFromTable(flags, &imageFormat));
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, givenUnalignedImageWidthAndNoSpaceInBufferForAlignmentWhenValidatingSurfaceFormatThenReturnError) {
    static_cast<MockDevice *>(context.getDevice(0))->deviceInfo.imagePitchAlignment = 128;
    imageDesc.image_width = 64;
    imageDesc.image_height = castToObject<Buffer>(imageDesc.mem_object)->getSize() / imageDesc.image_width;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    const auto surfaceFormat = static_cast<const SurfaceFormatInfo *>(Image::getSurfaceFormatFromTable(flags, &imageFormat));
    retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), surfaceFormat, &imageDesc, NULL);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(Image2dFromBufferTest, ExtensionString) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    std::string extensions = caps.deviceExtensions;
    size_t found = extensions.find("cl_khr_image2d_from_buffer");
    EXPECT_NE(std::string::npos, found);
}

TEST_F(Image2dFromBufferTest, InterceptBuffersHostPtr) {
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

TEST_F(Image2dFromBufferTest, givenMemoryManagerNotSupportingVirtualPaddingWhenImageIsCreatedThenPaddingIsNotApplied) {
    auto memoryManager = context.getMemoryManager();
    memoryManager->setVirtualPaddingSupport(false);

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(1, buffer->getRefInternalCount());

    std::unique_ptr<Image> imageFromBuffer(createImage());
    ASSERT_EQ(CL_SUCCESS, retVal);

    //graphics allocation for image and buffer is the same
    auto bufferGraphicsAllocation = buffer->getGraphicsAllocation();
    auto imageGraphicsAllocation = imageFromBuffer->getGraphicsAllocation();

    EXPECT_EQ(bufferGraphicsAllocation, imageGraphicsAllocation);
}

TEST_F(Image2dFromBufferTest, givenMemoryManagerSupportingVirtualPaddingWhenImageIsCreatedThatFitsInTheBufferThenPaddingIsNotApplied) {
    auto memoryManager = context.getMemoryManager();
    memoryManager->setVirtualPaddingSupport(true);

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);

    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(1, buffer->getRefInternalCount());

    std::unique_ptr<Image> imageFromBuffer(createImage());
    ASSERT_EQ(CL_SUCCESS, retVal);

    //graphics allocation for image and buffer is the same
    auto bufferGraphicsAllocation = buffer->getGraphicsAllocation();
    auto imageGraphicsAllocation = imageFromBuffer->getGraphicsAllocation();

    EXPECT_EQ(this->size, bufferGraphicsAllocation->getUnderlyingBufferSize());

    auto imgInfo = MockGmm::initImgInfo(imageDesc, 0, &imageFromBuffer->getSurfaceFormatInfo());
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_TRUE(queryGmm->gmmResourceInfo->getSizeAllocation() >= this->size);

    EXPECT_EQ(bufferGraphicsAllocation, imageGraphicsAllocation);
}

TEST_F(Image2dFromBufferTest, givenMemoryManagerSupportingVirtualPaddingWhenImageIsCreatedThatDoesntFitInTheBufferThenPaddingIsApplied) {
    imageFormat.image_channel_data_type = CL_FLOAT;
    imageFormat.image_channel_order = CL_RGBA;
    imageDesc.image_width = 29;
    imageDesc.image_height = 29;
    imageDesc.image_row_pitch = 512;

    //application calcualted buffer size
    auto bufferSize = imageDesc.image_row_pitch * imageDesc.image_height;
    auto buffer2 = clCreateBuffer(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, nullptr);

    auto storeMem = imageDesc.mem_object;

    imageDesc.mem_object = buffer2;

    auto memoryManager = context.getMemoryManager();
    memoryManager->setVirtualPaddingSupport(true);

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);

    std::unique_ptr<Image> imageFromBuffer(createImage());
    ASSERT_EQ(CL_SUCCESS, retVal);

    //graphics allocation for image and buffer is the same
    auto bufferGraphicsAllocation = buffer->getGraphicsAllocation();
    auto imageGraphicsAllocation = imageFromBuffer->getGraphicsAllocation();

    EXPECT_EQ(bufferSize, bufferGraphicsAllocation->getUnderlyingBufferSize());

    auto imgInfo = MockGmm::initImgInfo(imageDesc, 0, &imageFromBuffer->getSurfaceFormatInfo());
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_GT(queryGmm->gmmResourceInfo->getSizeAllocation(), bufferSize);

    EXPECT_NE(bufferGraphicsAllocation, imageGraphicsAllocation);
    EXPECT_EQ(queryGmm->gmmResourceInfo->getSizeAllocation(), imageFromBuffer->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(bufferSize, imageFromBuffer->getSize());
    imageDesc.mem_object = storeMem;
    clReleaseMemObject(buffer2);
}
TEST_F(Image2dFromBufferTest, givenMemoryManagerSupportingVirtualPaddingWhen1DImageFromBufferImageIsCreatedThenVirtualPaddingIsNotApplied) {
    imageFormat.image_channel_data_type = CL_FLOAT;
    imageFormat.image_channel_order = CL_RGBA;
    imageDesc.image_width = 1024;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;

    //application calcualted buffer size
    auto bufferSize = imageDesc.image_width * 16;
    auto buffer2 = clCreateBuffer(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, nullptr);
    auto storeMem = imageDesc.mem_object;
    imageDesc.mem_object = buffer2;

    auto memoryManager = context.getMemoryManager();
    memoryManager->setVirtualPaddingSupport(true);

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);

    std::unique_ptr<Image> imageFromBuffer(createImage());
    ASSERT_EQ(CL_SUCCESS, retVal);

    //graphics allocation match
    auto bufferGraphicsAllocation = buffer->getGraphicsAllocation();
    auto imageGraphicsAllocation = imageFromBuffer->getGraphicsAllocation();

    EXPECT_EQ(bufferGraphicsAllocation, imageGraphicsAllocation);
    imageDesc.mem_object = storeMem;
    clReleaseMemObject(buffer2);
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

    EXPECT_TRUE(imageFromBuffer.get()->isImageFromBuffer());
    EXPECT_TRUE(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY == imageFromBuffer.get()->getGraphicsAllocation()->getAllocationType());

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
