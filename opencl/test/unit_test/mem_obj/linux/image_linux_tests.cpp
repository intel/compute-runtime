/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/sharings/unified/unified_sharing_fixtures.h"

using namespace NEO;

using ImageLinuxTests = Test<ClDeviceFixture>;

TEST_F(ImageLinuxTests, givenPropertiesWithNtHandleWhenValidateAndCreateImageThenInvalidPropertyIsSet) {
    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x1234, 0};

    cl_image_desc imageDesc = {};
    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    std::unique_ptr<MockContext> context;
    context.reset(new MockContext(pClDevice));
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_int retVal = CL_SUCCESS;

    auto image = ImageFunctions::validateAndCreateImage(context.get(), properties, flags, 0, &imageFormat, &imageDesc, nullptr, retVal);

    EXPECT_EQ(retVal, CL_INVALID_PROPERTY);
    EXPECT_EQ(nullptr, image);

    clReleaseMemObject(image);
}

using UnifiedSharingImageTests = UnifiedSharingFixture<true, true>;

TEST_F(UnifiedSharingImageTests, givenPropertiesWithDmaBufWhenValidateAndCreateImageThenCorrectImageIsSet) {
    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, 0x1234, 0};

    cl_image_desc imageDesc = {};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64;
    imageDesc.image_height = 64;
    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_int retVal = CL_INVALID_VALUE;

    auto image = ImageFunctions::validateAndCreateImage(context.get(), properties, flags, 0, &imageFormat, &imageDesc, nullptr, retVal);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(image, nullptr);

    clReleaseMemObject(image);
}

using ImageMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(ImageMultiRootDeviceTests, givenDeviceHandleListWhenValidatingAndCreatingImageThenHandleIsImportedOnlyForListedDevices) {
    cl_mem_properties properties[] = {
        CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, 0x1234,
        CL_MEM_DEVICE_HANDLE_LIST_KHR, reinterpret_cast<cl_mem_properties>(static_cast<cl_device_id>(device2)),
        CL_MEM_DEVICE_HANDLE_LIST_END_KHR,
        0};

    cl_image_desc imageDesc = {};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64;
    imageDesc.image_height = 64;
    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;
    cl_int retVal = CL_INVALID_VALUE;

    auto clImage = ImageFunctions::validateAndCreateImage(context.get(), properties, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    auto image = castToObject<Image>(clImage);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(nullptr, image->getGraphicsAllocation(device1->getRootDeviceIndex()));
    EXPECT_NE(nullptr, image->getGraphicsAllocation(device2->getRootDeviceIndex()));

    clReleaseMemObject(clImage);
}
