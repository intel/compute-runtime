/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

struct GetSupportedImageFormatsTest : public PlatformFixture,
                                      public ContextFixture,
                                      public ::testing::TestWithParam<std::tuple<uint64_t, uint32_t>> {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

    GetSupportedImageFormatsTest() {
    }

    void SetUp() override {
        PlatformFixture::SetUp();
        ContextFixture::SetUp(num_devices, devices);
    }

    void TearDown() override {
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
};

TEST_P(GetSupportedImageFormatsTest, WhenGettingNumImageFormatsThenGreaterThanZeroIsReturned) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();
    retVal = pContext->getSupportedImageFormats(
        &castToObject<ClDevice>(devices[0])->getDevice(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numImageFormats, 0u);
}

TEST_P(GetSupportedImageFormatsTest, WhenRetrievingImageFormatsThenListIsNonEmpty) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();
    retVal = pContext->getSupportedImageFormats(
        &castToObject<ClDevice>(devices[0])->getDevice(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = pContext->getSupportedImageFormats(
        &castToObject<ClDevice>(devices[0])->getDevice(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);
    }

    retVal = pContext->getSupportedImageFormats(
        &castToObject<ClDevice>(devices[0])->getDevice(),
        CL_MEM_KERNEL_READ_AND_WRITE,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] imageFormatList;
}

TEST_P(GetSupportedImageFormatsTest, WhenRetrievingImageFormatsSRGBThenListIsNonEmpty) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool sRGBAFormatFound = false;
    bool sBGRAFormatFound = false;
    bool isReadOnly = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();
    retVal = pContext->getSupportedImageFormats(
        &castToObject<ClDevice>(devices[0])->getDevice(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = pContext->getSupportedImageFormats(
        &castToObject<ClDevice>(devices[0])->getDevice(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    isReadOnly |= (imageFormatsFlags == CL_MEM_READ_ONLY);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_sRGBA) {
            sRGBAFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_sBGRA) {
            sBGRAFormatFound = true;
        }
    }

    if (isReadOnly && ((&castToObject<ClDevice>(devices[0])->getDevice())->getHardwareInfo().capabilityTable.supportsOcl21Features)) {
        EXPECT_TRUE(sRGBAFormatFound & sBGRAFormatFound);
    } else {
        EXPECT_FALSE(sRGBAFormatFound | sBGRAFormatFound);
    }

    delete[] imageFormatList;
}

TEST(ImageFormats, WhenCheckingIsDepthFormatThenCorrectValueReturned) {
    for (auto &format : SurfaceFormats::readOnly20()) {
        EXPECT_FALSE(Image::isDepthFormat(format.OCLImageFormat));
    }

    for (auto &format : SurfaceFormats::readOnlyDepth()) {
        EXPECT_TRUE(Image::isDepthFormat(format.OCLImageFormat));
    }
}

struct PackedYuvExtensionSupportedImageFormatsTest : public ::testing::TestWithParam<std::tuple<uint64_t, uint32_t>> {

    void SetUp() override {
        device = std::make_unique<MockClDevice>(new MockDevice());
        context = std::unique_ptr<MockContext>(new MockContext(device.get(), true));
    }

    void TearDown() override {
    }
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    cl_int retVal;
};

TEST_P(PackedYuvExtensionSupportedImageFormatsTest, WhenRetrievingImageFormatsPackedYUVThenListIsNonEmpty) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool yuyvFormatFound = false;
    bool uyvyFormatFound = false;
    bool yvyuFormatFound = false;
    bool vyuyFormatFound = false;
    bool isReadOnly = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->deviceInfo.nv12Extension = false;
    device->deviceInfo.packedYuvExtension = true;

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    isReadOnly |= (imageFormatsFlags == CL_MEM_READ_ONLY);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_YUYV_INTEL) {
            yuyvFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_UYVY_INTEL) {
            uyvyFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_YVYU_INTEL) {
            yvyuFormatFound = true;
        }

        if (imageFormatList[entry].image_channel_order == CL_VYUY_INTEL) {
            vyuyFormatFound = true;
        }
    }

    if (isReadOnly && imageFormats == CL_MEM_OBJECT_IMAGE2D) {
        EXPECT_TRUE(yuyvFormatFound);
        EXPECT_TRUE(uyvyFormatFound);
        EXPECT_TRUE(yvyuFormatFound);
        EXPECT_TRUE(vyuyFormatFound);
    } else {
        EXPECT_FALSE(yuyvFormatFound);
        EXPECT_FALSE(uyvyFormatFound);
        EXPECT_FALSE(yvyuFormatFound);
        EXPECT_FALSE(vyuyFormatFound);
    }

    delete[] imageFormatList;
}

struct NV12ExtensionSupportedImageFormatsTest : public ::testing::TestWithParam<std::tuple<uint64_t, uint32_t>> {

    void SetUp() override {
        device = std::make_unique<MockClDevice>(new MockDevice());
        context = std::unique_ptr<MockContext>(new MockContext(device.get(), true));
    }

    void TearDown() override {
    }
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    cl_int retVal;
};

typedef NV12ExtensionSupportedImageFormatsTest NV12ExtensionUnsupportedImageFormatsTest;

TEST_P(NV12ExtensionSupportedImageFormatsTest, givenNV12ExtensionWhenQueriedForImageFormatsThenNV12FormatIsReturnedOnlyFor2DImages) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool nv12FormatFound = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->deviceInfo.nv12Extension = true;
    device->deviceInfo.packedYuvExtension = false;

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);

    auto supportsOcl20Features = device->getHardwareInfo().capabilityTable.supportsOcl21Features;
    size_t expectedNumReadOnlyFormats = (supportsOcl20Features) ? SurfaceFormats::readOnly20().size() : SurfaceFormats::readOnly12().size();

    if (Image::isImage2dOr2dArray(imageFormats) && imageFormatsFlags == CL_MEM_READ_ONLY) {
        expectedNumReadOnlyFormats += SurfaceFormats::readOnlyDepth().size();
    }

    if (Image::isImage2d(imageFormats)) {
        if (imageFormatsFlags == CL_MEM_READ_ONLY) {
            EXPECT_EQ(expectedNumReadOnlyFormats + SurfaceFormats::planarYuv().size(), static_cast<size_t>(numImageFormats));
        }
        if (imageFormatsFlags == CL_MEM_NO_ACCESS_INTEL) {
            EXPECT_EQ(expectedNumReadOnlyFormats + SurfaceFormats::planarYuv().size(), static_cast<size_t>(numImageFormats));
        }
    } else {
        if (imageFormatsFlags == CL_MEM_READ_ONLY) {
            EXPECT_EQ(expectedNumReadOnlyFormats, static_cast<size_t>(numImageFormats));
        }
        if (imageFormatsFlags == CL_MEM_NO_ACCESS_INTEL) {
            EXPECT_EQ(expectedNumReadOnlyFormats, static_cast<size_t>(numImageFormats));
        }
    }

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_NV12_INTEL) {
            nv12FormatFound = true;
        }
    }

    if (imageFormats == CL_MEM_OBJECT_IMAGE2D) {
        EXPECT_TRUE(nv12FormatFound);
    } else {
        EXPECT_FALSE(nv12FormatFound);
    }

    delete[] imageFormatList;
}

TEST_P(NV12ExtensionUnsupportedImageFormatsTest, givenNV12ExtensionWhenQueriedForWriteOnlyOrReadWriteImageFormatsThenNV12FormatIsNotReturned) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    bool nv12FormatFound = false;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->deviceInfo.nv12Extension = true;

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);

    if (imageFormatsFlags == CL_MEM_WRITE_ONLY) {
        if (!Image::isImage2dOr2dArray(imageFormats)) {
            EXPECT_EQ(SurfaceFormats::writeOnly().size(), static_cast<size_t>(numImageFormats));
        } else {
            EXPECT_EQ(SurfaceFormats::writeOnly().size() + SurfaceFormats::readWriteDepth().size(),
                      static_cast<size_t>(numImageFormats));
        }
    }

    if (imageFormatsFlags == CL_MEM_READ_WRITE) {
        if (!Image::isImage2dOr2dArray(imageFormats)) {
            EXPECT_EQ(SurfaceFormats::readWrite().size(), static_cast<size_t>(numImageFormats));
        } else {
            EXPECT_EQ(SurfaceFormats::readWrite().size() + SurfaceFormats::readWriteDepth().size(),
                      static_cast<size_t>(numImageFormats));
        }
    }

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);

        if (imageFormatList[entry].image_channel_order == CL_NV12_INTEL) {
            nv12FormatFound = true;
        }
    }

    EXPECT_FALSE(nv12FormatFound);

    delete[] imageFormatList;
}

TEST_P(NV12ExtensionSupportedImageFormatsTest, WhenRetrievingLessImageFormatsThanAvailableThenListIsNonEmpty) {
    cl_uint numImageFormats = 0;
    uint64_t imageFormatsFlags;
    uint32_t imageFormats;
    std::tie(imageFormatsFlags, imageFormats) = GetParam();

    device->deviceInfo.nv12Extension = true;

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        0,
        nullptr,
        &numImageFormats);
    EXPECT_GT(numImageFormats, 0u);

    if (numImageFormats > 1)
        numImageFormats--;

    auto imageFormatList = new cl_image_format[numImageFormats];
    memset(imageFormatList, 0, numImageFormats * sizeof(cl_image_format));

    retVal = context->getSupportedImageFormats(
        &device->getDevice(),
        imageFormatsFlags,
        imageFormats,
        numImageFormats,
        imageFormatList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (cl_uint entry = 0; entry < numImageFormats; ++entry) {
        EXPECT_NE(0u, imageFormatList[entry].image_channel_order);
        EXPECT_NE(0u, imageFormatList[entry].image_channel_data_type);
    }

    delete[] imageFormatList;
}

cl_mem_flags GetSupportedImageFormatsFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY};

cl_mem_object_type GetSupportedImageFormats[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D};

INSTANTIATE_TEST_CASE_P(
    Context,
    GetSupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(GetSupportedImageFormatsFlags),
        ::testing::ValuesIn(GetSupportedImageFormats)));

INSTANTIATE_TEST_CASE_P(
    Context,
    PackedYuvExtensionSupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(GetSupportedImageFormatsFlags),
        ::testing::ValuesIn(GetSupportedImageFormats)));

cl_mem_flags NV12ExtensionSupportedImageFormatsFlags[] = {
    CL_MEM_NO_ACCESS_INTEL,
    CL_MEM_READ_ONLY};

cl_mem_flags NV12ExtensionUnsupportedImageFormatsFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY};

cl_mem_object_type NV12ExtensionSupportedImageFormats[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE2D};

INSTANTIATE_TEST_CASE_P(
    Context,
    NV12ExtensionSupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(NV12ExtensionSupportedImageFormatsFlags),
        ::testing::ValuesIn(NV12ExtensionSupportedImageFormats)));

INSTANTIATE_TEST_CASE_P(
    Context,
    NV12ExtensionUnsupportedImageFormatsTest,
    ::testing::Combine(
        ::testing::ValuesIn(NV12ExtensionUnsupportedImageFormatsFlags),
        ::testing::ValuesIn(NV12ExtensionSupportedImageFormats)));
