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
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj_helper.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

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
