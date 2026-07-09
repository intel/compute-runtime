/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj_helper.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"

#include "CL/cl.h"

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
        return ZE_RESULT_SUCCESS;
    }

    bool createImageCalled = false;
    size_t capturedRowPitch = 0;
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

using MemObjHelperTest = LeoMemObjApiFixture;

TEST_F(MemObjHelperTest, givenContextDeviceWhenValidateMemoryPropertiesForBufferThenDeviceIsAssociated) {
    auto &neoDeviceRef = context->getClDevice()->getDevice();
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &neoDeviceRef);

    bool valid = MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, CL_MEM_READ_WRITE, 0, *context);
    EXPECT_TRUE(valid);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
