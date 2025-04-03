/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mem_obj/image_compression_fixture.h"

XE_HPG_CORETEST_F(ImageCompressionTests, givenDifferentImageFormatsWhenCreatingImageThenCompressionIsCorrectlySet) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    flags = CL_MEM_READ_ONLY;
    MockContext context;

    struct ImageFormatCompression {
        cl_image_format imageFormat;
        bool isCompressable;
    };
    const std::vector<ImageFormatCompression> imageFormats = {
        {{CL_LUMINANCE, CL_UNORM_INT8}, false},
        {{CL_LUMINANCE, CL_UNORM_INT16}, false},
        {{CL_LUMINANCE, CL_HALF_FLOAT}, false},
        {{CL_LUMINANCE, CL_FLOAT}, false},
        {{CL_INTENSITY, CL_UNORM_INT8}, false},
        {{CL_INTENSITY, CL_UNORM_INT16}, false},
        {{CL_INTENSITY, CL_HALF_FLOAT}, false},
        {{CL_INTENSITY, CL_FLOAT}, false},
        {{CL_A, CL_UNORM_INT16}, false},
        {{CL_A, CL_HALF_FLOAT}, false},
        {{CL_A, CL_FLOAT}, false},
        {{CL_R, CL_UNSIGNED_INT8}, true},
        {{CL_R, CL_UNSIGNED_INT16}, true},
        {{CL_R, CL_UNSIGNED_INT32}, true},
        {{CL_RG, CL_UNSIGNED_INT32}, true},
        {{CL_RGBA, CL_UNSIGNED_INT32}, true}};

    for (const auto &format : imageFormats) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &format.imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        auto image = std::unique_ptr<Image>(Image::create(
            mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));

        ASSERT_NE(nullptr, image);
        EXPECT_TRUE(myMemoryManager->mockMethodCalled);

        auto compressionAllowed = !context.getDevice(0)->getProductHelper().isCompressionForbidden(*defaultHwInfo);
        if (compressionAllowed) {
            EXPECT_EQ(format.isCompressable, myMemoryManager->capturedPreferCompressed);
        } else {
            EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
        }
    }
}

XE_HPG_CORETEST_F(ImageCompressionTests, givenRedescribableFormatWhenCreatingAllocationThenCompressionIsCorrectlySet) {
    MockContext context{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);

    auto compressionAllowed = !context.getDevice(0)->getProductHelper().isCompressionForbidden(*defaultHwInfo);
    if (compressionAllowed) {
        EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, myMemoryManager->capturedPreferCompressed);
    } else {
        EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
    }

    imageFormat.image_channel_order = CL_RG;
    surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    image = std::unique_ptr<Image>(Image::create(
        mockContext.get(), ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);

    if (compressionAllowed) {
        EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, myMemoryManager->capturedPreferCompressed);
    } else {
        EXPECT_FALSE(myMemoryManager->capturedPreferCompressed);
    }
}
