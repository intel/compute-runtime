/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_device.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ClCreateImageTests {

template <typename T>
struct clCreateImageTests : public api_fixture,
                            public T {

    void SetUp() override {
        api_fixture::SetUp();

        // clang-format off
        imageFormat.image_channel_order     = CL_RGBA;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;

        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = 32;
        imageDesc.image_height      = 32;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object        = nullptr;
        // clang-format on
    }

    void TearDown() override {
        api_fixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

typedef clCreateImageTests<::testing::Test> clCreateImageTest;

TEST_F(clCreateImageTest, GivenNullHostPtrWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(clCreateImageTest, GivenDeviceThatDoesntSupportImagesWhenCreatingTiledImageThenInvalidOperationErrorIsReturned) {
    auto device = static_cast<MockDevice *>(pContext->getDevice(0));
    device->deviceInfo.imageSupport = CL_FALSE;
    cl_bool imageSupportInfo = CL_TRUE;
    auto status = clGetDeviceInfo(devices[0], CL_DEVICE_IMAGE_SUPPORT, sizeof(imageSupportInfo), &imageSupportInfo, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    cl_bool expectedValue = CL_FALSE;
    EXPECT_EQ(expectedValue, imageSupportInfo);
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    if (UnitTestHelper<FamilyType>::tiledImagesSupported) {
        EXPECT_EQ(CL_INVALID_OPERATION, retVal);
        EXPECT_EQ(nullptr, image);
    } else {
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);

        retVal = clReleaseMemObject(image);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

HWTEST_F(clCreateImageTest, GivenDeviceThatDoesntSupportImagesWhenCreatingNonTiledImageThenCreate) {
    auto device = static_cast<MockDevice *>(pContext->getDevice(0));
    device->deviceInfo.imageSupport = CL_FALSE;
    cl_bool imageSupportInfo = CL_TRUE;
    auto status = clGetDeviceInfo(devices[0], CL_DEVICE_IMAGE_SUPPORT, sizeof(imageSupportInfo), &imageSupportInfo, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    cl_bool expectedValue = CL_FALSE;
    EXPECT_EQ(expectedValue, imageSupportInfo);

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_height = 1;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenNonNullHostPtrAndAlignedRowPitchWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    char hostPtr[4096];
    imageDesc.image_row_pitch = 128;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenNonNullHostPtrAndUnalignedRowPitchWhenCreatingImageThenInvalidImageDescriptotErrorIsReturned) {
    char hostPtr[4096];
    imageDesc.image_row_pitch = 129;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenNonNullHostPtrAndSmallRowPitchWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    char hostPtr[4096];
    imageDesc.image_row_pitch = 4;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenUnrestrictedIntelFlagWhenCreatingImageWithInvalidFlagCombinationThenImageIsCreatedAndSuccessReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenNotNullHostPtrAndNoHostPtrFlagWhenCreatingImageThenInvalidHostPtrErrorIsReturned) {
    char hostPtr[4096];
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenInvalidFlagBitsWhenCreatingImageThenInvalidValueErrorIsReturned) {
    cl_mem_flags flags = (1 << 12);
    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenInvalidFlagBitsWhenCreatingImageFromAnotherImageThenInvalidValueErrorIsReturned) {
    imageFormat.image_channel_order = CL_NV12_INTEL;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageFormat.image_channel_order = CL_RG;
    imageDesc.mem_object = image;
    cl_mem_flags flags = (1 << 30);

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenInvalidRowPitchWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    imageDesc.image_row_pitch = 655;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenNullHostPtrAndCopyHostPtrFlagWhenCreatingImageThenInvalidHostPtrErrorIsReturned) {
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenNullHostPtrAndMemUseHostPtrFlagWhenCreatingImageThenInvalidHostPtrErrorIsReturned) {
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageTest, GivenNullHostPtrAndNonZeroRowPitchWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    imageDesc.image_row_pitch = 4;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenNonZeroPitchWhenCreatingImageFromBufferThenImageIsCreatedAndSuccessReturned) {

    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 4096 * 9, nullptr, nullptr);
    auto &helper = HwHelper::get(renderCoreFamily);
    HardwareInfo hardwareInfo = *platformDevices[0];

    imageDesc.mem_object = buffer;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 17;
    imageDesc.image_height = 17;
    imageDesc.image_row_pitch = helper.getPitchAlignmentForImage(&hardwareInfo) * 17;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_NE(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, GivenNotNullHostPtrAndRowPitchIsNotGreaterThanWidthTimesElementSizeWhenCreatingImageThenInvalidImageDescriptorErrorIsReturned) {
    imageDesc.image_row_pitch = 64;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, GivenNullContextWhenCreatingImageThenInvalidContextErrorIsReturned) {
    auto image = clCreateImage(
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

typedef clCreateImageTests<::testing::Test> clCreateImageTestYUV;
TEST_F(clCreateImageTestYUV, GivenInvalidGlagWhenCreatingYuvImageThenInvalidValueErrorIsReturned) {
    imageFormat.image_channel_order = CL_YUYV_INTEL;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTestYUV, Given1DImageTypeWhenCreatingYuvImageThenInvalidImageDescriptorErrorIsReturned) {
    imageFormat.image_channel_order = CL_YUYV_INTEL;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

typedef clCreateImageTests<::testing::TestWithParam<uint64_t>> clCreateImageValidFlags;

static cl_mem_flags validFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY,
    CL_MEM_USE_HOST_PTR,
    CL_MEM_ALLOC_HOST_PTR,
    CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_NO_ACCESS,
    CL_MEM_NO_ACCESS_INTEL,
    CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL,
};

INSTANTIATE_TEST_CASE_P(CreateImageWithFlags,
                        clCreateImageValidFlags,
                        ::testing::ValuesIn(validFlags));

TEST_P(clCreateImageValidFlags, GivenValidFlagsWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {
    cl_mem_flags flags = GetParam();
    std::unique_ptr<char[]> ptr;
    char *hostPtr = nullptr;

    if (flags & CL_MEM_USE_HOST_PTR ||
        flags & CL_MEM_COPY_HOST_PTR) {
        ptr = std::make_unique<char[]>(alignUp(imageDesc.image_width * imageDesc.image_height * 4, MemoryConstants::pageSize));
        hostPtr = ptr.get();
    }

    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        hostPtr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

typedef clCreateImageTests<::testing::TestWithParam<uint64_t>> clCreateImageInvalidFlags;

static cl_mem_flags invalidFlagsCombinations[] = {
    CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
    CL_MEM_READ_WRITE | CL_MEM_READ_ONLY,
    CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY,
    CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR,
    CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS,
    CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
    CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_WRITE,
    CL_MEM_NO_ACCESS_INTEL | CL_MEM_WRITE_ONLY,
    CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_ONLY};

INSTANTIATE_TEST_CASE_P(CreateImageWithFlags,
                        clCreateImageInvalidFlags,
                        ::testing::ValuesIn(invalidFlagsCombinations));

TEST_P(clCreateImageInvalidFlags, GivenInvalidFlagsCombinationsWhenCreatingImageThenInvalidValueErrorIsReturned) {

    char ptr[10];
    imageDesc.image_row_pitch = 128;
    cl_mem_flags flags = GetParam();

    auto image = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        ptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

struct ImageFlags {
    cl_mem_flags parentFlags;
    cl_mem_flags flags;
};

static ImageFlags flagsWithUnrestrictedIntel[] = {
    {CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, CL_MEM_READ_WRITE},
    {CL_MEM_READ_WRITE, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL}};

typedef clCreateImageTests<::testing::TestWithParam<ImageFlags>> clCreateImageFlagsUnrestrictedIntel;

INSTANTIATE_TEST_CASE_P(CreateImageWithFlags,
                        clCreateImageFlagsUnrestrictedIntel,
                        ::testing::ValuesIn(flagsWithUnrestrictedIntel));

TEST_P(clCreateImageFlagsUnrestrictedIntel, GivenFlagsIncludingUnrestrictedIntelWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {

    imageFormat.image_channel_order = CL_NV12_INTEL;
    ImageFlags imageFlags = GetParam();
    cl_mem_flags parentFlags = imageFlags.parentFlags;
    cl_mem_flags flags = imageFlags.flags;

    auto image = clCreateImage(
        pContext,
        parentFlags | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_RG;

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

static ImageFlags validFlagsAndParentFlags[] = {
    {CL_MEM_WRITE_ONLY, CL_MEM_HOST_NO_ACCESS},
    {CL_MEM_READ_ONLY, CL_MEM_HOST_NO_ACCESS},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_HOST_NO_ACCESS},
    {CL_MEM_HOST_NO_ACCESS, CL_MEM_READ_WRITE}};

typedef clCreateImageTests<::testing::TestWithParam<ImageFlags>> clCreateImageValidFlagsAndParentFlagsCombinations;

INSTANTIATE_TEST_CASE_P(CreateImageWithFlags,
                        clCreateImageValidFlagsAndParentFlagsCombinations,
                        ::testing::ValuesIn(validFlagsAndParentFlags));

TEST_P(clCreateImageValidFlagsAndParentFlagsCombinations, GivenValidFlagsAndParentFlagsWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {

    imageFormat.image_channel_order = CL_NV12_INTEL;
    ImageFlags imageFlags = GetParam();
    cl_mem_flags parentFlags = imageFlags.parentFlags;
    cl_mem_flags flags = imageFlags.flags;

    auto image = clCreateImage(
        pContext,
        parentFlags | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_RG;

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

static ImageFlags invalidFlagsAndParentFlags[] = {
    {CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE},
    {CL_MEM_WRITE_ONLY, CL_MEM_READ_ONLY},
    {CL_MEM_READ_ONLY, CL_MEM_READ_WRITE},
    {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_READ_WRITE},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_WRITE_ONLY},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_READ_ONLY},
    {CL_MEM_HOST_NO_ACCESS, CL_MEM_HOST_WRITE_ONLY},
    {CL_MEM_HOST_NO_ACCESS, CL_MEM_HOST_READ_ONLY}};

typedef clCreateImageTests<::testing::TestWithParam<ImageFlags>> clCreateImageInvalidFlagsAndParentFlagsCombinations;

INSTANTIATE_TEST_CASE_P(CreateImageWithFlags,
                        clCreateImageInvalidFlagsAndParentFlagsCombinations,
                        ::testing::ValuesIn(invalidFlagsAndParentFlags));

TEST_P(clCreateImageInvalidFlagsAndParentFlagsCombinations, GivenInvalidFlagsAndParentFlagsWhenCreatingImageThenInvalidMemObjectErrorIsReturned) {

    imageFormat.image_channel_order = CL_NV12_INTEL;
    ImageFlags imageFlags = GetParam();
    cl_mem_flags parentFlags = imageFlags.parentFlags;
    cl_mem_flags flags = imageFlags.flags;

    auto image = clCreateImage(
        pContext,
        parentFlags | CL_MEM_HOST_NO_ACCESS,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    imageFormat.image_channel_order = CL_RG;
    imageDesc.mem_object = image;

    auto imageFromImageObject = clCreateImage(
        pContext,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

struct ImageSizes {
    size_t width;
    size_t height;
    size_t depth;
};

ImageSizes validImage2DSizes[] = {{64, 64, 1}, {3, 3, 1}, {8192, 1, 1}, {117, 39, 1}, {16384, 4, 1}, {4, 16384, 1}};

typedef clCreateImageTests<::testing::TestWithParam<ImageSizes>> clCreateImageValidSizesTest;

INSTANTIATE_TEST_CASE_P(validImage2DSizes,
                        clCreateImageValidSizesTest,
                        ::testing::ValuesIn(validImage2DSizes));

TEST_P(clCreateImageValidSizesTest, GivenValidSizesWhenCreatingImageThenImageIsCreatedAndSuccessReturned) {

    ImageSizes sizes = GetParam();
    imageDesc.image_width = sizes.width;
    imageDesc.image_height = sizes.height;
    imageDesc.image_depth = sizes.depth;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(image);
}

typedef clCreateImageTests<::testing::Test> clCreateImage2DTest;

TEST_F(clCreateImage2DTest, GivenValidParametersWhenCreating2DImageThenImageIsCreatedAndSuccessReturned) {
    auto image = clCreateImage2D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage2DTest, GivenNoPtrToReturnValueWhenCreating2DImageThenImageIsCreated) {
    auto image = clCreateImage2D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        0,
        0,
        nullptr);

    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage2DTest, GivenInvalidContextsWhenCreating2DImageThenInvalidContextErrorIsReturned) {
    auto image = clCreateImage2D(
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);
}

typedef clCreateImageTests<::testing::Test> clCreateImage3DTest;

TEST_F(clCreateImage3DTest, GivenValidParametersWhenCreating3DImageThenImageIsCreatedAndSuccessReturned) {
    auto image = clCreateImage3D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        1,
        0,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage3DTest, GivenNoPtrToReturnValueWhenCreating3DImageThenImageIsCreated) {
    auto image = clCreateImage3D(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        1,
        0,
        0,
        0,
        nullptr);

    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImage3DTest, GivenInvalidContextsWhenCreating3DImageThenInvalidContextErrorIsReturned) {
    auto image = clCreateImage3D(
        nullptr,
        CL_MEM_READ_WRITE,
        &imageFormat,
        10,
        10,
        1,
        0,
        0,
        0,
        &retVal);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);
}

using clCreateImageWithPropertiesINTELTest = clCreateImageTest;

TEST_F(clCreateImageWithPropertiesINTELTest, GivenInvalidContextWhenCreatingImageWithPropertiesThenInvalidContextErrorIsReturned) {

    auto image = clCreateImageWithPropertiesINTEL(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, image);
}

TEST_F(clCreateImageWithPropertiesINTELTest, GivenValidParametersWhenCreatingImageWithPropertiesThenImageIsCreatedAndSuccessReturned) {

    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE, 0};

    auto image = clCreateImageWithPropertiesINTEL(
        pContext,
        properties,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageWithPropertiesINTELTest, GivenInvalidPropertyKeyWhenCreatingImageWithPropertiesThenInvalidValueErrorIsReturned) {

    cl_mem_properties_intel properties[] = {(cl_mem_properties_intel(1) << 31), 0, 0};

    auto image = clCreateImageWithPropertiesINTEL(
        pContext,
        properties,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

typedef clCreateImageTests<::testing::Test> clCreateImageFromImageTest;

TEST_F(clCreateImageFromImageTest, GivenImage2dWhenCreatingImage2dFromImageWithTheSameDescriptorAndValidFormatThenImageIsCreatedAndSuccessReturned) {

    imageFormat.image_channel_order = CL_BGRA;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_width), &imageDesc.image_width, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_height), &imageDesc.image_height, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof(imageDesc.image_depth), &imageDesc.image_depth, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, sizeof(imageDesc.image_row_pitch), &imageDesc.image_row_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, sizeof(imageDesc.image_slice_pitch), &imageDesc.image_slice_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, sizeof(imageDesc.num_mip_levels), &imageDesc.num_mip_levels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, sizeof(imageDesc.num_samples), &imageDesc.num_samples, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, sizeof(imageDesc.image_array_size), &imageDesc.image_array_size, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_sBGRA;

    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(imageFromImageObject);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageFromImageTest, GivenImage2dWhenCreatingImage2dFromImageWithDifferentDescriptorAndValidFormatThenInvalidImageFormatDescriptorErrorIsReturned) {

    imageFormat.image_channel_order = CL_BGRA;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_width), &imageDesc.image_width, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_height), &imageDesc.image_height, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof(imageDesc.image_depth), &imageDesc.image_depth, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, sizeof(imageDesc.image_row_pitch), &imageDesc.image_row_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, sizeof(imageDesc.image_slice_pitch), &imageDesc.image_slice_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, sizeof(imageDesc.num_mip_levels), &imageDesc.num_mip_levels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, sizeof(imageDesc.num_samples), &imageDesc.num_samples, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, sizeof(imageDesc.image_array_size), &imageDesc.image_array_size, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    imageDesc.mem_object = image;
    imageDesc.image_width++;
    imageFormat.image_channel_order = CL_sBGRA;

    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageFromImageTest, GivenImage2dWhenCreatingImage2dFromImageWithTheSameDescriptorAndNotValidFormatThenInvalidImageFormatDescriptorErrorIsReturned) {

    imageFormat.image_channel_order = CL_BGRA;

    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_width), &imageDesc.image_width, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(imageDesc.image_height), &imageDesc.image_height, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof(imageDesc.image_depth), &imageDesc.image_depth, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, sizeof(imageDesc.image_row_pitch), &imageDesc.image_row_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, sizeof(imageDesc.image_slice_pitch), &imageDesc.image_slice_pitch, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, sizeof(imageDesc.num_mip_levels), &imageDesc.num_mip_levels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, sizeof(imageDesc.num_samples), &imageDesc.num_samples, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, sizeof(imageDesc.image_array_size), &imageDesc.image_array_size, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    imageDesc.mem_object = image;
    imageFormat.image_channel_order = CL_BGRA;

    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

uint32_t non2dImageTypes[] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE1D_BUFFER, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D};

struct clCreateNon2dImageFromImageTest : public clCreateImageFromImageTest,
                                         public ::testing::WithParamInterface<uint32_t /*image type*/> {
    void SetUp() override {
        clCreateImageFromImageTest::SetUp();
        image = clCreateImage(
            pContext,
            CL_MEM_READ_ONLY,
            &imageFormat,
            &imageDesc,
            nullptr,
            &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);
        imageDesc.mem_object = image;
    }
    void TearDown() override {
        retVal = clReleaseMemObject(image);
        clCreateImageFromImageTest::TearDown();
    }
    cl_mem image;
};

TEST_P(clCreateNon2dImageFromImageTest, GivenImage2dWhenCreatingImageFromNon2dImageThenInvalidImageDescriptorErrorIsReturned) {

    imageDesc.image_type = GetParam();
    auto imageFromImageObject = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, imageFromImageObject);
}

INSTANTIATE_TEST_CASE_P(clCreateNon2dImageFromImageTests,
                        clCreateNon2dImageFromImageTest,
                        ::testing::ValuesIn(non2dImageTypes));
} // namespace ClCreateImageTests
