/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
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
