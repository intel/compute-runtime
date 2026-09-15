/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj_helper.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct LeoMemObjApiFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        device = platform->getDevices()[0].get();
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, nullptr, 1, &clDevice, true);
    }

    void TearDown() override {
        context.reset();
        Test<OclFixture>::TearDown();
    }

    void setSupportsImages(bool supported) {
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.supportsImages = supported;
    }

    ClDevice *device = nullptr;
    std::unique_ptr<Context> context;
};

struct CreateBufferWithUnalignedHostPtrTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        leoContext = std::make_unique<Context>(nullptr, context->toHandle(), 1, &clDeviceId, true);
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
        svmAllocsManager = driverHandle->getSvmAllocsManager();

        basePtr = alignedMalloc(allocSize + 2 * MemoryConstants::pageSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, basePtr);
        unalignedHostPtr = ptrOffset(basePtr, MemoryConstants::cacheLineSize);
    }

    void TearDown() override {
        alignedFree(basePtr);
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    static constexpr size_t allocSize = 2 * MemoryConstants::pageSize;

    ClDevice *clDevice = nullptr;
    std::unique_ptr<Context> leoContext;
    SVMAllocsManager *svmAllocsManager = nullptr;
    void *basePtr = nullptr;
    void *unalignedHostPtr = nullptr;
};

TEST_F(CreateBufferWithUnalignedHostPtrTest, givenPageUnalignedHostPtrWhenCreateBufferThenApplicationStorageIsWrappedWithoutCopy) {
    cl_int retVal = CL_INVALID_VALUE;
    auto clBuffer = clCreateBuffer(leoContext.get(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, allocSize, unalignedHostPtr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, clBuffer);

    auto buffer = castToObject<Buffer>(clBuffer);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(unalignedHostPtr, buffer->getCpuPtr());

    auto svmData = svmAllocsManager->getSVMAlloc(buffer->getUsmPtr());
    ASSERT_NE(nullptr, svmData);

    auto allocation = svmData->gpuAllocations.getGraphicsAllocation(clDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(unalignedHostPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(MemoryConstants::cacheLineSize, allocation->getAllocationOffset());
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize>(allocation->getGpuAddressWithoutOffset()));

    auto usmPtr = buffer->getUsmPtr();
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(clBuffer));
    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(usmPtr));
}

TEST_F(CreateBufferWithUnalignedHostPtrTest, givenPageAlignedHostPtrWhenCreateBufferThenApplicationStorageIsWrappedWithoutCopy) {
    cl_int retVal = CL_INVALID_VALUE;
    auto clBuffer = clCreateBuffer(leoContext.get(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, allocSize, basePtr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, clBuffer);

    auto buffer = castToObject<Buffer>(clBuffer);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(basePtr, buffer->getCpuPtr());

    auto svmData = svmAllocsManager->getSVMAlloc(buffer->getUsmPtr());
    ASSERT_NE(nullptr, svmData);

    auto allocation = svmData->gpuAllocations.getGraphicsAllocation(clDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(basePtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(0u, allocation->getAllocationOffset());

    auto usmPtr = buffer->getUsmPtr();
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(clBuffer));
    EXPECT_EQ(nullptr, svmAllocsManager->getSVMAlloc(usmPtr));
}

using GetSupportedImageFormatsTest = LeoMemObjApiFixture;

TEST_F(GetSupportedImageFormatsTest, givenImagesNotSupportedWhenGetSupportedImageFormatsThenZeroFormatsReturned) {
    setSupportsImages(false);

    cl_uint numImageFormats = 0xdeadu;
    auto retVal = clGetSupportedImageFormats(context.get(), CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE3D,
                                             0, nullptr, &numImageFormats);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(GetSupportedImageFormatsTest, givenImagesSupportedWhenGetSupportedImageFormatsThenNonZeroFormatsReturned) {
    setSupportsImages(true);

    cl_uint numImageFormats = 0;
    auto retVal = clGetSupportedImageFormats(context.get(), CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE3D,
                                             0, nullptr, &numImageFormats);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, numImageFormats);
}

struct CreateImageWithoutSupportTest : LeoMemObjApiFixture {
    void SetUp() override {
        LeoMemObjApiFixture::SetUp();
        setSupportsImages(false);
        imageFormat.image_channel_order = CL_RGBA;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = 4;
        imageDesc.image_height = 4;
    }

    cl_image_format imageFormat{};
    cl_image_desc imageDesc{};
};

TEST_F(CreateImageWithoutSupportTest, givenImagesNotSupportedWhenCreateImageThenInvalidOperationReturnedAndNoImageCreated) {
    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImage(context.get(), CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(CreateImageWithoutSupportTest, givenImagesNotSupportedWhenCreateImageWithPropertiesThenInvalidOperationReturnedAndNoImageCreated) {
    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImageWithProperties(context.get(), nullptr, CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(CreateImageWithoutSupportTest, givenImagesNotSupportedWhenCreateImageWithPropertiesINTELThenInvalidOperationReturnedAndNoImageCreated) {
    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImageWithPropertiesINTEL(context.get(), nullptr, CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(CreateImageWithoutSupportTest, givenImagesNotSupportedWhenCreateImage2DThenInvalidOperationReturnedAndNoImageCreated) {
    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImage2D(context.get(), CL_MEM_READ_WRITE, &imageFormat, 4, 4, 0, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(CreateImageWithoutSupportTest, givenImagesNotSupportedWhenCreateImage3DThenInvalidOperationReturnedAndNoImageCreated) {
    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImage3D(context.get(), CL_MEM_READ_WRITE, &imageFormat, 4, 4, 4, 0, 0, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(CreateImageWithoutSupportTest, givenImagesNotSupportedWhenCreateImageWithoutErrcodeRetThenNoImageCreated) {
    auto image = clCreateImage(context.get(), CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, nullptr);
    EXPECT_EQ(nullptr, image);
}

struct RowPitchForImageFromBufferTest : ::testing::Test {
    cl_image_desc makeImage2dFromBufferDesc(size_t width) {
        cl_image_desc imageDesc{};
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = width;
        imageDesc.image_height = 100;
        imageDesc.image_row_pitch = 0;
        imageDesc.mem_object = reinterpret_cast<cl_mem>(0x1); // non-null; never dereferenced
        return imageDesc;
    }

    cl_image_format imageFormat{CL_R, CL_UNORM_INT8};
};

TEST_F(RowPitchForImageFromBufferTest, givenImage2dFromBufferWithSingleByteElementAndZeroRowPitchThenPitchIsContiguous) {
    auto imageDesc = makeImage2dFromBufferDesc(64);
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    EXPECT_EQ(64u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &imageFormat, &imageDesc));
}

TEST_F(RowPitchForImageFromBufferTest, givenImage2dFromBufferWithMultiByteElementAndZeroRowPitchThenPitchIsContiguous) {
    auto imageDesc = makeImage2dFromBufferDesc(64);
    imageFormat.image_channel_data_type = CL_UNORM_INT16;
    EXPECT_EQ(128u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &imageFormat, &imageDesc));
}

TEST_F(RowPitchForImageFromBufferTest, givenExplicitRowPitchThenItIsHonored) {
    auto imageDesc = makeImage2dFromBufferDesc(64);
    imageDesc.image_row_pitch = 256;
    EXPECT_EQ(256u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &imageFormat, &imageDesc));
}

TEST_F(RowPitchForImageFromBufferTest, given1dBufferImageWithZeroRowPitchThenPitchIsContiguous) {
    auto imageDesc = makeImage2dFromBufferDesc(64);
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    EXPECT_EQ(64u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &imageFormat, &imageDesc));
}

TEST_F(RowPitchForImageFromBufferTest, givenNoBackingBufferThenRowPitchIsNotComputed) {
    auto imageDesc = makeImage2dFromBufferDesc(64);
    imageDesc.mem_object = nullptr;
    EXPECT_EQ(0u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &imageFormat, &imageDesc));
}

struct CapturingImageL0Context : public L0::ult::ContextStubMock {
    using L0::Context::devices;

    ze_result_t createImage(ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_handle_t *phImage) override {
        createImageCalled = true;
        for (auto *base = reinterpret_cast<const ze_base_desc_t *>(desc->pNext); base != nullptr;
             base = reinterpret_cast<const ze_base_desc_t *>(base->pNext)) {
            if (base->stype == ZE_STRUCTURE_TYPE_CUSTOM_PITCH_EXP_DESC) {
                capturedRowPitch = reinterpret_cast<const ze_custom_pitch_exp_desc_t *>(base)->rowPitch;
            }
        }
        *phImage = nullptr; // null handle -> Image destructor skips zeImageDestroy
        return createImageResult;
    }

    bool createImageCalled = false;
    size_t capturedRowPitch = 0;
    ze_result_t createImageResult = ZE_RESULT_SUCCESS;
};

struct CreateImageFromBufferWiringTest : LeoMemObjApiFixture {
    void SetUp() override {
        LeoMemObjApiFixture::SetUp();
        setSupportsImages(true);
    }
};

TEST_F(CreateImageFromBufferWiringTest, givenImage2dFromBufferWithZeroRowPitchWhenCreateImageThenContiguousCustomPitchPassedToLevelZero) {
    CapturingImageL0Context mockL0Context{};
    mockL0Context.devices[device->getRootDeviceIndex()] = device->getL0Handle();

    cl_device_id clDeviceId = device;
    auto leoContext = std::make_unique<Context>(nullptr, mockL0Context.toHandle(), 1, &clDeviceId, true);

    const size_t width = 64;
    const size_t height = 100;
    MemoryProperties bufferProperties{};
    uint64_t dummyBufferStorage = 0;
    Buffer buffer(leoContext.get(), bufferProperties, CL_MEM_READ_WRITE, &dummyBufferStorage, nullptr, width * height, true);

    cl_image_format imageFormat{CL_R, CL_UNORM_INT8};
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_row_pitch = 0;
    imageDesc.mem_object = static_cast<cl_mem>(&buffer);

    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImage(leoContext.get(), CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_TRUE(mockL0Context.createImageCalled);
    EXPECT_EQ(width * 1u, mockL0Context.capturedRowPitch);

    clReleaseMemObject(image);
}

TEST_F(CreateImageFromBufferWiringTest, givenImage1dBufferFromBufferWhenCreateImageThenParentBufferIsRetainedWithoutChangingItsApiRefCount) {
    CapturingImageL0Context mockL0Context{};
    mockL0Context.devices[device->getRootDeviceIndex()] = device->getL0Handle();

    cl_device_id clDeviceId = device;
    auto leoContext = std::make_unique<Context>(nullptr, mockL0Context.toHandle(), 1, &clDeviceId, true);

    const size_t width = 64;
    const size_t elementSize = 4;
    MemoryProperties bufferProperties{};
    uint64_t dummyBufferStorage = 0;
    Buffer buffer(leoContext.get(), bufferProperties, CL_MEM_READ_WRITE, &dummyBufferStorage, nullptr, width * elementSize, true);
    const auto refCountWithoutImage = buffer.getRefInternalCount();

    cl_image_format imageFormat{CL_RGBA, CL_SIGNED_INT8};
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    imageDesc.image_width = width;
    imageDesc.mem_object = static_cast<cl_mem>(&buffer);

    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImage(leoContext.get(), CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(refCountWithoutImage + 1, buffer.getRefInternalCount());

    cl_uint bufferApiRefCount = 0;
    EXPECT_EQ(CL_SUCCESS, clGetMemObjectInfo(static_cast<cl_mem>(&buffer), CL_MEM_REFERENCE_COUNT,
                                             sizeof(bufferApiRefCount), &bufferApiRefCount, nullptr));
    EXPECT_EQ(1u, bufferApiRefCount);

    clReleaseMemObject(image);
    EXPECT_EQ(refCountWithoutImage, buffer.getRefInternalCount());
}

TEST_F(CreateImageFromBufferWiringTest, givenImage2dFromBufferWhenCreateImageThenParentBufferIsRetained) {
    CapturingImageL0Context mockL0Context{};
    mockL0Context.devices[device->getRootDeviceIndex()] = device->getL0Handle();

    cl_device_id clDeviceId = device;
    auto leoContext = std::make_unique<Context>(nullptr, mockL0Context.toHandle(), 1, &clDeviceId, true);

    const size_t width = 64;
    const size_t height = 100;
    MemoryProperties bufferProperties{};
    uint64_t dummyBufferStorage = 0;
    Buffer buffer(leoContext.get(), bufferProperties, CL_MEM_READ_WRITE, &dummyBufferStorage, nullptr, width * height, true);
    const auto refCountWithoutImage = buffer.getRefInternalCount();

    cl_image_format imageFormat{CL_R, CL_UNORM_INT8};
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.mem_object = static_cast<cl_mem>(&buffer);

    cl_int retVal = CL_SUCCESS;
    auto image = clCreateImage(leoContext.get(), CL_MEM_READ_WRITE, &imageFormat, &imageDesc, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(refCountWithoutImage + 1, buffer.getRefInternalCount());

    clReleaseMemObject(image);
    EXPECT_EQ(refCountWithoutImage, buffer.getRefInternalCount());
}

struct CreateImageValidationTest : LeoMemObjApiFixture {
    void SetUp() override {
        LeoMemObjApiFixture::SetUp();
        setSupportsImages(true);
        mockL0Context.devices[device->getRootDeviceIndex()] = device->getL0Handle();
        cl_device_id clDeviceId = device;
        leoContext = std::make_unique<Context>(nullptr, mockL0Context.toHandle(), 1, &clDeviceId, true);
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = 4;
        imageDesc.image_height = 4;
    }

    void TearDown() override {
        leoContext.reset();
        LeoMemObjApiFixture::TearDown();
    }

    cl_mem createImage(cl_mem_flags flags, void *hostPtr = nullptr) {
        return clCreateImage(leoContext.get(), flags, &imageFormat, &imageDesc, hostPtr, &retVal);
    }

    CapturingImageL0Context mockL0Context{};
    std::unique_ptr<Context> leoContext;
    cl_image_format imageFormat{CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imageDesc{};
    cl_int retVal = CL_SUCCESS;
};

TEST_F(CreateImageValidationTest, givenPackedYuvFormatWithUnsupportedChannelTypeWhenCreateImageThenInvalidImageFormatDescriptorReturned) {
    imageFormat = {CL_YUYV_INTEL, CL_UNSIGNED_INT16};
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenZeroWidthWhenCreateImageThenInvalidImageDescriptorReturnedAndLevelZeroNotReached) {
    imageDesc.image_width = 0;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);
}

TEST_F(CreateImageValidationTest, givenZeroHeightWhenCreateImageThenInvalidImageDescriptorReturned) {
    imageDesc.image_height = 0;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenWidthExceedingDeviceLimitWhenCreateImageThenInvalidImageSizeReturned) {
    imageDesc.image_width = device->getSharedDeviceInfo().image2DMaxWidth + 1;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_F(CreateImageValidationTest, givenHeightExceedingDeviceLimitWhenCreateImageThenInvalidImageSizeReturned) {
    imageDesc.image_height = device->getSharedDeviceInfo().image2DMaxHeight + 1;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_F(CreateImageValidationTest, givenPackedYuvFormatWithOddWidthWhenCreateImageThenInvalidImageDescriptorReturned) {
    imageFormat = {CL_YUYV_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 7;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenPackedYuvFormatWithoutReadOnlyFlagWhenCreateImageThenInvalidValueReturned) {
    imageFormat = {CL_YUYV_INTEL, CL_UNORM_INT8};
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_WRITE));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(CreateImageValidationTest, givenRowPitchWithoutHostPtrWhenCreateImageThenInvalidImageDescriptorReturned) {
    imageDesc.image_row_pitch = 64;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenRowPitchNotMultipleOfElementSizeWhenCreateImageThenInvalidImageDescriptorReturned) {
    imageFormat = {CL_YUYV_INTEL, CL_UNORM_INT8};
    std::vector<uint8_t> hostMemory(1024, 0);
    imageDesc.image_row_pitch = 9;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, hostMemory.data()));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenRowPitchSmallerThanRowSizeWhenCreateImageThenInvalidImageDescriptorReturned) {
    std::vector<uint8_t> hostMemory(1024, 0);
    imageDesc.image_row_pitch = 4;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, hostMemory.data()));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenPlanarYuvFormatWithoutHostNoAccessFlagWhenCreateImageThenInvalidValueReturned) {
    imageFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(CreateImageValidationTest, givenPlanarYuvFormatWithWidthNotMultipleOfFourWhenCreateImageThenInvalidImageDescriptorReturned) {
    imageFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 7;
    imageDesc.image_height = 8;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenPlanarYuvFormatWithHeightNotMultipleOfFourWhenCreateImageThenInvalidImageDescriptorReturned) {
    imageFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 16;
    imageDesc.image_height = 17;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_F(CreateImageValidationTest, givenPlanarYuvFormatExceedingPlanarYuvLimitWhenCreateImageThenInvalidImageSizeReturned) {
    imageFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 4;
    imageDesc.image_height = static_cast<size_t>(device->getDeviceInfo().planarYuvMaxHeight) + 4;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_F(CreateImageValidationTest, givenPlaneImageWithAccessFlagsConflictingWithParentWhenCreateImageThenInvalidValueReturned) {
    imageFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 8;
    imageDesc.image_height = 8;
    auto parent = createImage(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, parent);

    // A plane of a read only parent cannot be made writable.
    imageFormat = {CL_R, CL_UNORM_INT8};
    imageDesc = {};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.mem_object = parent;

    EXPECT_EQ(nullptr, createImage(CL_MEM_WRITE_ONLY));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_WRITE));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // Host pointer flags are never allowed on an image created from another mem object.
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // A plane of a host-no-access parent cannot widen host access.
    EXPECT_EQ(nullptr, createImage(CL_MEM_HOST_READ_ONLY));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    clReleaseMemObject(parent);
}

TEST_F(CreateImageValidationTest, givenValidPlanarYuvDescriptorWhenCreateImageThenImageIsCreated) {
    imageFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 64;
    imageDesc.image_height = 32;

    auto image = createImage(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(mockL0Context.createImageCalled);

    clReleaseMemObject(image);
}

TEST_F(CreateImageValidationTest, givenValidPackedYuvDescriptorWhenCreateImageThenImageIsCreated) {
    imageFormat = {CL_YUYV_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 64;
    imageDesc.image_height = 32;

    auto image = createImage(CL_MEM_READ_ONLY);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(mockL0Context.createImageCalled);

    clReleaseMemObject(image);
}

TEST_F(CreateImageValidationTest, givenLegalAndHardwareBackedFormatsWhenCreateImageThenImageIsCreated) {
    const cl_image_format formats[] = {
        {CL_R, CL_FLOAT},
        {CL_INTENSITY, CL_UNORM_INT8},
        {CL_BGRA, CL_UNORM_INT8},
        {CL_sRGBA, CL_UNORM_INT8}};

    for (const auto &format : formats) {
        imageFormat = format;
        auto image = createImage(CL_MEM_READ_ONLY);
        EXPECT_EQ(CL_SUCCESS, retVal) << "channel order " << format.image_channel_order;
        ASSERT_NE(nullptr, image) << "channel order " << format.image_channel_order;
        clReleaseMemObject(image);
    }
}

TEST_F(CreateImageValidationTest, givenLegalFormatWithoutHardwareSupportWhenCreateImageThenFormatNotSupportedReturned) {
    const cl_image_format formats[] = {
        {CL_RGB, CL_UNORM_SHORT_565},
        {CL_Rx, CL_UNORM_INT8}};

    for (const auto &format : formats) {
        imageFormat = format;
        EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
        EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal) << "channel order " << format.image_channel_order;
    }
}

TEST_F(CreateImageValidationTest, givenLevelZeroImageCreationFailureWhenCreateImageThenNoImageIsReturnedAndErrorIsPropagated) {
    mockL0Context.createImageResult = ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_TRUE(mockL0Context.createImageCalled);
}

TEST_F(CreateImageValidationTest, givenIllegalChannelOrderAndDataTypeCombinationWhenCreateImageThenInvalidImageFormatDescriptorReturned) {
    const cl_image_format formats[] = {
        {CL_INTENSITY, CL_SIGNED_INT8},
        {CL_LUMINANCE, CL_UNSIGNED_INT32},
        {CL_RGB, CL_FLOAT},
        {CL_BGRA, CL_UNORM_INT16},
        {CL_sRGB, CL_UNORM_INT16},
        {CL_DEPTH, CL_UNORM_INT8},
        {CL_DEPTH_STENCIL, CL_UNORM_INT16},
        {CL_NV12_INTEL, CL_HALF_FLOAT},
        {0, CL_UNORM_INT8}};

    for (const auto &format : formats) {
        imageFormat = format;
        EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY));
        EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal)
            << "channel order " << format.image_channel_order
            << ", data type " << format.image_channel_data_type;
    }
}

TEST_F(CreateImageValidationTest, givenNullImageFormatWhenCreateImageThenInvalidImageFormatDescriptorReturned) {
    retVal = CL_SUCCESS;
    EXPECT_EQ(nullptr, clCreateImage(leoContext.get(), CL_MEM_READ_ONLY, nullptr, &imageDesc, nullptr, &retVal));
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);
}

TEST_F(CreateImageValidationTest, givenNullImageDescriptorWhenCreateImageThenInvalidImageDescriptorReturned) {
    retVal = CL_SUCCESS;
    EXPECT_EQ(nullptr, clCreateImage(leoContext.get(), CL_MEM_READ_ONLY, &imageFormat, nullptr, nullptr, &retVal));
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);
}

TEST_F(CreateImageValidationTest, givenHostPtrFlagWithoutHostPtrWhenCreateImageThenInvalidHostPtrReturned) {
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, nullptr));
    EXPECT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);

    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, nullptr));
    EXPECT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);
}

TEST_F(CreateImageValidationTest, givenPlanarYuvFormatWithCopyHostPtrFlagAndNoHostPtrWhenCreateImageThenInvalidHostPtrReturnedBeforeAnyPlaneIsWritten) {
    // Without this check the UV plane write offsets a null host pointer and hands the result to
    // Level Zero, which cannot reject it as null.
    imageFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    imageDesc.image_width = 64;
    imageDesc.image_height = 32;

    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR, nullptr));
    EXPECT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);
}

TEST_F(CreateImageValidationTest, givenHostPtrWithoutHostPtrFlagWhenCreateImageThenInvalidHostPtrReturned) {
    std::vector<uint8_t> hostMemory(1024, 0);
    EXPECT_EQ(nullptr, createImage(CL_MEM_READ_ONLY, hostMemory.data()));
    EXPECT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);
}

TEST_F(CreateImageValidationTest, givenUnsupportedPropertyWhenCreateImageWithPropertiesThenInvalidPropertyReturned) {
    const cl_mem_properties properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY, 0};
    retVal = CL_SUCCESS;
    EXPECT_EQ(nullptr, clCreateImageWithProperties(leoContext.get(), properties, CL_MEM_READ_ONLY,
                                                   &imageFormat, &imageDesc, nullptr, &retVal));
    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
    EXPECT_FALSE(mockL0Context.createImageCalled);
}

struct CreateImageFromBufferValidationTest : CreateImageValidationTest {
    void SetUp() override {
        CreateImageValidationTest::SetUp();
        parentBuffer = std::make_unique<Buffer>(leoContext.get(), bufferProperties, CL_MEM_READ_WRITE,
                                                &dummyBufferStorage, nullptr, sizeof(dummyBufferStorage), true);
        imageDesc.mem_object = static_cast<cl_mem>(parentBuffer.get());
    }

    void TearDown() override {
        parentBuffer.reset();
        CreateImageValidationTest::TearDown();
    }

    MemoryProperties bufferProperties{};
    uint64_t dummyBufferStorage[64] = {};
    std::unique_ptr<Buffer> parentBuffer;
};

TEST_F(CreateImageFromBufferValidationTest, givenIllegalChannelDataTypeWhenCreateImageFromBufferThenInvalidImageFormatDescriptorReturnedAndLevelZeroNotReached) {
    // An unrecognised pair maps onto no Level Zero layout, so it must be rejected here rather than
    // reaching zeImageCreate with ZE_IMAGE_FORMAT_LAYOUT_FORCE_UINT32.
    const cl_image_format formats[] = {
        {CL_RGBA, 0xdeadbeef},
        {0xdeadbeef, CL_UNORM_INT8},
        {CL_INTENSITY, CL_SIGNED_INT8}};

    for (const auto &format : formats) {
        imageFormat = format;
        EXPECT_EQ(nullptr, createImage(CL_MEM_READ_WRITE))
            << "channel order " << format.image_channel_order
            << ", data type " << format.image_channel_data_type;
        EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal)
            << "channel order " << format.image_channel_order
            << ", data type " << format.image_channel_data_type;
        EXPECT_FALSE(mockL0Context.createImageCalled);
    }
}

TEST_F(CreateImageFromBufferValidationTest, givenLegalFormatWhenCreateImageFromBufferThenDescriptorChecksAreNotApplied) {
    // The extents come from the parent buffer, so the standalone descriptor rules must not fire.
    imageDesc.image_width = 8;
    imageDesc.image_height = 8;

    auto image = createImage(CL_MEM_READ_WRITE);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(mockL0Context.createImageCalled);

    clReleaseMemObject(image);
}

using MemObjHelperTest = LeoMemObjApiFixture;

TEST_F(MemObjHelperTest, givenContextDeviceWhenValidateMemoryPropertiesForBufferThenDeviceIsAssociated) {
    auto &neoDeviceRef = context->getClDevice()->getDevice();
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &neoDeviceRef);

    bool valid = MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, CL_MEM_READ_WRITE, 0, *context);
    EXPECT_TRUE(valid);
}

struct LeoNv12ImageTest : LeoMemObjApiFixture {
    void SetUp() override {
        LeoMemObjApiFixture::SetUp();
        if (!device->getHardwareInfo().capabilityTable.supportsImages) {
            GTEST_SKIP() << "Product does not support images";
        }
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDevice, true);
        ASSERT_EQ(CL_SUCCESS, context->initialize());
        nv12Format.image_channel_order = CL_NV12_INTEL;
        nv12Format.image_channel_data_type = CL_UNORM_INT8;
        nv12Desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        nv12Desc.image_width = width;
        nv12Desc.image_height = height;
    }

    cl_mem createParent() {
        cl_int err = CL_INVALID_VALUE;
        auto parent = clCreateImage(context.get(),
                                    CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL,
                                    &nv12Format, &nv12Desc, nullptr, &err);
        EXPECT_EQ(CL_SUCCESS, err);
        return parent;
    }

    cl_mem createPlane(cl_mem parent, cl_channel_order order, size_t planeIndex, cl_int *err) {
        cl_image_format planeFormat{};
        planeFormat.image_channel_order = order;
        planeFormat.image_channel_data_type = CL_UNORM_INT8;
        cl_image_desc planeDesc{};
        planeDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        planeDesc.mem_object = parent;
        planeDesc.image_depth = planeIndex;
        return clCreateImage(context.get(), CL_MEM_READ_WRITE, &planeFormat, &planeDesc, nullptr, err);
    }

    static constexpr size_t width = 16;
    static constexpr size_t height = 16;
    cl_image_format nv12Format{};
    cl_image_desc nv12Desc{};
};

TEST_F(LeoNv12ImageTest, givenNV12FormatWhenCreatingParentImageThenImageIsCreated) {
    auto parent = createParent();
    ASSERT_NE(nullptr, parent);
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(parent));
}

TEST_F(LeoNv12ImageTest, givenNV12ParentWhenExtractingYPlaneThenDimensionsMatchParentAndParentIsAssociated) {
    auto parent = createParent();
    ASSERT_NE(nullptr, parent);

    cl_int err = CL_INVALID_VALUE;
    auto yPlane = createPlane(parent, CL_R, 0, &err);
    ASSERT_EQ(CL_SUCCESS, err);
    ASSERT_NE(nullptr, yPlane);

    size_t planeWidth = 0;
    size_t planeHeight = 0;
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(yPlane, CL_IMAGE_WIDTH, sizeof(planeWidth), &planeWidth, nullptr));
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(yPlane, CL_IMAGE_HEIGHT, sizeof(planeHeight), &planeHeight, nullptr));
    EXPECT_EQ(width, planeWidth);
    EXPECT_EQ(height, planeHeight);

    cl_mem associated = nullptr;
    EXPECT_EQ(CL_SUCCESS, clGetMemObjectInfo(yPlane, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(associated), &associated, nullptr));
    EXPECT_EQ(parent, associated);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(yPlane));
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(parent));
}

TEST_F(LeoNv12ImageTest, givenNV12ParentWhenExtractingUVPlaneThenDimensionsAreHalved) {
    auto parent = createParent();
    ASSERT_NE(nullptr, parent);

    cl_int err = CL_INVALID_VALUE;
    auto uvPlane = createPlane(parent, CL_RG, 1, &err);
    ASSERT_EQ(CL_SUCCESS, err);
    ASSERT_NE(nullptr, uvPlane);

    size_t planeWidth = 0;
    size_t planeHeight = 0;
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(uvPlane, CL_IMAGE_WIDTH, sizeof(planeWidth), &planeWidth, nullptr));
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(uvPlane, CL_IMAGE_HEIGHT, sizeof(planeHeight), &planeHeight, nullptr));
    EXPECT_EQ(width / 2, planeWidth);
    EXPECT_EQ(height / 2, planeHeight);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(uvPlane));
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(parent));
}

TEST_F(LeoNv12ImageTest, givenInvalidPlaneIndexWhenExtractingPlaneThenInvalidImageDescriptorReturned) {
    auto parent = createParent();
    ASSERT_NE(nullptr, parent);

    cl_int err = CL_SUCCESS;
    auto badPlane = createPlane(parent, CL_R, 2, &err);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, err);
    EXPECT_EQ(nullptr, badPlane);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(parent));
}

TEST_F(LeoNv12ImageTest, givenParentReleasedBeforePlaneThenPlaneKeepsParentAlive) {
    auto parent = createParent();
    ASSERT_NE(nullptr, parent);

    cl_int err = CL_INVALID_VALUE;
    auto yPlane = createPlane(parent, CL_R, 0, &err);
    ASSERT_EQ(CL_SUCCESS, err);
    ASSERT_NE(nullptr, yPlane);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(parent));

    size_t planeWidth = 0;
    EXPECT_EQ(CL_SUCCESS, clGetImageInfo(yPlane, CL_IMAGE_WIDTH, sizeof(planeWidth), &planeWidth, nullptr));
    EXPECT_EQ(width, planeWidth);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(yPlane));
}

struct WhiteBoxNv12Context : public Context {
    using Context::Context;
    using Context::internalCopyCmdLists;
};

struct LeoNv12HostPtrImageTest : LeoNv12ImageTest {
    void SetUp() override {
        LeoNv12ImageTest::SetUp();
        if (IsSkipped()) {
            return;
        }
        cl_device_id clDevice = device;
        hostPtrContext = std::make_unique<WhiteBoxNv12Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDevice, true);
        hostPtrContext->internalCopyCmdLists[device->getRootDeviceIndex()] = capturingCmdList.toHandle();
    }

    void TearDown() override {
        if (hostPtrContext) {
            hostPtrContext->internalCopyCmdLists.clear();
            hostPtrContext.reset();
        }
        LeoNv12ImageTest::TearDown();
    }

    CapturingCommandList capturingCmdList{};
    std::unique_ptr<WhiteBoxNv12Context> hostPtrContext;
};

TEST_F(LeoNv12HostPtrImageTest, givenNV12ImageCreatedWithHostPtrThenYAndUVPlanesAreWritten) {
    std::vector<uint8_t> hostData((width * height * 3) / 2, 0x80);

    cl_int err = CL_INVALID_VALUE;
    auto parent = clCreateImage(hostPtrContext.get(),
                                CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_COPY_HOST_PTR,
                                &nv12Format, &nv12Desc, hostData.data(), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    ASSERT_NE(nullptr, parent);

    ASSERT_EQ(2u, capturingCmdList.appendImageCopyFromMemoryExtArgs.count());

    const auto &yWrite = capturingCmdList.appendImageCopyFromMemoryExtArgs[0];
    ASSERT_TRUE(yWrite.dstRegion.has_value());
    EXPECT_EQ(width, yWrite.dstRegion->width);
    EXPECT_EQ(height, yWrite.dstRegion->height);
    EXPECT_EQ(width, yWrite.srcRowPitch);
    EXPECT_EQ(static_cast<const void *>(hostData.data()), yWrite.srcptr);

    const auto &uvWrite = capturingCmdList.appendImageCopyFromMemoryExtArgs[1];
    ASSERT_TRUE(uvWrite.dstRegion.has_value());
    EXPECT_EQ(width / 2, uvWrite.dstRegion->width);
    EXPECT_EQ(height / 2, uvWrite.dstRegion->height);
    EXPECT_EQ(width, uvWrite.srcRowPitch);
    EXPECT_EQ(static_cast<const void *>(hostData.data() + width * height), uvWrite.srcptr);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(parent));
}

TEST_F(LeoNv12HostPtrImageTest, givenNV12ImageCreatedWithHostPtrAndRowPitchThenUVPlaneOffsetUsesRowPitch) {
    const size_t rowPitch = width + 32;
    nv12Desc.image_row_pitch = rowPitch;
    std::vector<uint8_t> hostData(rowPitch * height * 3 / 2, 0x40);

    cl_int err = CL_INVALID_VALUE;
    auto parent = clCreateImage(hostPtrContext.get(),
                                CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_COPY_HOST_PTR,
                                &nv12Format, &nv12Desc, hostData.data(), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    ASSERT_NE(nullptr, parent);

    ASSERT_EQ(2u, capturingCmdList.appendImageCopyFromMemoryExtArgs.count());
    EXPECT_EQ(static_cast<uint32_t>(rowPitch), capturingCmdList.appendImageCopyFromMemoryExtArgs[0].srcRowPitch);
    EXPECT_EQ(static_cast<uint32_t>(rowPitch), capturingCmdList.appendImageCopyFromMemoryExtArgs[1].srcRowPitch);
    EXPECT_EQ(static_cast<const void *>(hostData.data() + rowPitch * height), capturingCmdList.appendImageCopyFromMemoryExtArgs[1].srcptr);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(parent));
}

TEST_F(LeoNv12HostPtrImageTest, givenHostPtrCopyFailureWhenCreateImageThenNoImageIsReturnedAndErrorIsPropagated) {
    // A half written image must not escape to the caller alongside an error code.
    capturingCmdList.appendImageCopyFromMemoryExtResult = ZE_RESULT_ERROR_DEVICE_LOST;
    std::vector<uint8_t> hostData((width * height * 3) / 2, 0x80);

    cl_int err = CL_SUCCESS;
    auto parent = clCreateImage(hostPtrContext.get(),
                                CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_COPY_HOST_PTR,
                                &nv12Format, &nv12Desc, hostData.data(), &err);

    EXPECT_EQ(nullptr, parent);
    EXPECT_EQ(CL_DEVICE_NOT_AVAILABLE, err);
    EXPECT_TRUE(capturingCmdList.appendImageCopyFromMemoryExtArgs.wasCalled());
}

struct WhiteBoxHostPtrContext : public Context {
    using Context::Context;
    using Context::internalCopyCmdLists;
};

struct LeoZeroCopyUseHostPtrTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        leoContext = std::make_unique<WhiteBoxHostPtrContext>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);
        leoContext->internalCopyCmdLists[clDevice->getRootDeviceIndex()] = capturingCmdList.toHandle();
        svmManager = leoContext->getL0Object()->getDriverHandle()->getSvmAllocsManager();

        hostPtr = alignedMalloc(bufferSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, hostPtr);
        setIntegratedDevice(true);
    }

    void TearDown() override {
        alignedFree(hostPtr);
        leoContext->internalCopyCmdLists.clear();
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    void setIntegratedDevice(bool integrated) {
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = integrated;
    }

    Buffer *createBufferFromHostPtr(void *ptr, size_t size, cl_mem_flags extraFlags = 0) {
        cl_int errcode = CL_INVALID_VALUE;
        auto buffer = clCreateBuffer(leoContext.get(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | extraFlags, size, ptr, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        return castToObject<Buffer>(buffer);
    }

    static constexpr size_t bufferSize = 2 * MemoryConstants::pageSize;

    ClDevice *clDevice = nullptr;
    CapturingCommandList capturingCmdList{};
    std::unique_ptr<WhiteBoxHostPtrContext> leoContext;
    SVMAllocsManager *svmManager = nullptr;
    void *hostPtr = nullptr;
};

TEST_F(LeoZeroCopyUseHostPtrTest, givenIntegratedDeviceAndAlignedHostPtrWhenCreatingBufferWithUseHostPtrThenHostStorageIsImportedAndNoCopyIsAppended) {
    auto buffer = createBufferFromHostPtr(hostPtr, bufferSize);
    ASSERT_NE(nullptr, buffer);

    EXPECT_EQ(hostPtr, buffer->getUsmPtr());
    EXPECT_EQ(hostPtr, buffer->getCpuPtr());
    EXPECT_FALSE(buffer->getUsesSvm());
    EXPECT_FALSE(capturingCmdList.appendMemoryCopyArgs.wasCalled());

    auto allocData = svmManager->getSVMAlloc(hostPtr);
    ASSERT_NE(nullptr, allocData);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(hostPtr), allocData->allocationFlagsProperty.hostptr);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoZeroCopyUseHostPtrTest, givenForceHostMemoryOnDiscreteDeviceWhenCreatingBufferWithUseHostPtrThenHostStorageIsImported) {
    setIntegratedDevice(false);

    auto buffer = createBufferFromHostPtr(hostPtr, bufferSize, CL_MEM_FORCE_HOST_MEMORY_INTEL);
    ASSERT_NE(nullptr, buffer);

    EXPECT_EQ(hostPtr, buffer->getUsmPtr());
    EXPECT_FALSE(capturingCmdList.appendMemoryCopyArgs.wasCalled());

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoZeroCopyUseHostPtrTest, givenIntegratedDeviceAndHostPtrNotAlignedToCacheLineWhenCreatingBufferWithUseHostPtrThenStorageIsAllocatedAndCopied) {
    auto misalignedPtr = ptrOffset(hostPtr, 1u);

    auto buffer = createBufferFromHostPtr(misalignedPtr, MemoryConstants::cacheLineSize);
    ASSERT_NE(nullptr, buffer);

    EXPECT_NE(misalignedPtr, buffer->getUsmPtr());
    EXPECT_EQ(misalignedPtr, buffer->getCpuPtr());

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(static_cast<const void *>(misalignedPtr), capturingCmdList.appendMemoryCopyArgs[0].srcptr);
    EXPECT_EQ(buffer->getUsmPtr(), capturingCmdList.appendMemoryCopyArgs[0].dstptr);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoZeroCopyUseHostPtrTest, givenIntegratedDeviceAndSizeNotAlignedToCacheLineWhenCreatingBufferWithUseHostPtrThenStorageIsAllocatedAndCopied) {
    auto unalignedSize = MemoryConstants::cacheLineSize - 1u;

    auto buffer = createBufferFromHostPtr(hostPtr, unalignedSize);
    ASSERT_NE(nullptr, buffer);

    EXPECT_NE(hostPtr, buffer->getUsmPtr());
    EXPECT_EQ(hostPtr, buffer->getCpuPtr());
    EXPECT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoZeroCopyUseHostPtrTest, givenDiscreteDeviceWhenCreatingBufferWithUseHostPtrThenStorageIsAllocatedAndCopied) {
    setIntegratedDevice(false);

    auto buffer = createBufferFromHostPtr(hostPtr, bufferSize);
    ASSERT_NE(nullptr, buffer);

    EXPECT_NE(hostPtr, buffer->getUsmPtr());
    EXPECT_EQ(hostPtr, buffer->getCpuPtr());
    EXPECT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(hostPtr));

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(LeoZeroCopyUseHostPtrTest, givenZeroCopyForUseHostPtrDisabledWhenCreatingBufferWithUseHostPtrThenStorageIsAllocatedAndCopied) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableZeroCopyForUseHostPtr.set(true);

    auto buffer = createBufferFromHostPtr(hostPtr, bufferSize);
    ASSERT_NE(nullptr, buffer);

    EXPECT_NE(hostPtr, buffer->getUsmPtr());
    EXPECT_EQ(hostPtr, buffer->getCpuPtr());
    EXPECT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
