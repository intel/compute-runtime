/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/mocks/mock_device.h"

using namespace OCLRT;

namespace ULT {

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

TEST_F(clCreateImageTest, returnsSuccess) {
    char ptr[10];
    imageDesc.image_row_pitch = 128;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        ptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateImageTest, InvalidFlagsCombinationWithFlagClMemAccessFlagsUnrestrictedIntel) {
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

TEST_F(clCreateImageTest, InvalidFlagBits) {
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

TEST_F(clCreateImageTest, CreateImageWithInvalidFlagBitsFromAnotherImage) {
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
    cl_mem_flags flags = (1 << 3);

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

TEST_F(clCreateImageTest, NotNullHostPtrAndRowPitchIsNotMultipleOfElementSizeReturnsError) {
    imageDesc.image_row_pitch = 655;
    char ptr[10];
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        ptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, NullHostPtrAndRowPitchIsNotZeroReturnsError) {
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

TEST_F(clCreateImageTest, givenImage2DFromBufferInputParamsWhenImageIsCreatedThenNonZeroRowPitchIsAccepted) {

    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 4096 * 9, nullptr, nullptr);

    imageDesc.mem_object = buffer;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 17;
    imageDesc.image_height = 17;
    imageDesc.image_row_pitch = 4 * 17;

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

TEST_F(clCreateImageTest, NotNullHostPtrAndRowPitchIsNotGreaterThanWidthTimesElementSizeReturnsError) {
    imageDesc.image_row_pitch = 64;
    char ptr[10];
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        ptr,
        &retVal);
    ASSERT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    EXPECT_EQ(nullptr, image);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clCreateImageTest, nullContextReturnsError) {
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
TEST_F(clCreateImageTestYUV, invalidFlagReturnsError) {
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

TEST_F(clCreateImageTestYUV, invalidImageTypeReturnsError) {
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

TEST_P(clCreateImageValidFlags, validFlags) {

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

TEST_P(clCreateImageInvalidFlags, invalidFlagsCombinations) {

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

TEST_P(clCreateImageFlagsUnrestrictedIntel, flagsWithUnrestrictedIntel) {

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

TEST_P(clCreateImageValidFlagsAndParentFlagsCombinations, validFlagsAndParentFlags) {

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
    {CL_MEM_READ_ONLY, CL_MEM_READ_WRITE},
    {CL_MEM_NO_ACCESS_INTEL, CL_MEM_READ_WRITE},
    {CL_MEM_HOST_NO_ACCESS, CL_MEM_HOST_WRITE_ONLY}};

typedef clCreateImageTests<::testing::TestWithParam<ImageFlags>> clCreateImageInvalidFlagsAndParentFlagsCombinations;

INSTANTIATE_TEST_CASE_P(CreateImageWithFlags,
                        clCreateImageInvalidFlagsAndParentFlagsCombinations,
                        ::testing::ValuesIn(invalidFlagsAndParentFlags));

TEST_P(clCreateImageInvalidFlagsAndParentFlagsCombinations, invalidFlagsAndParentFlags) {

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

TEST_P(clCreateImageValidSizesTest, ValidSizesPassedToCreateImageReturnsImage) {

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

TEST_F(clCreateImage2DTest, GivenValidInputsWhenImageIsCreatedThenReturnsSuccess) {
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

TEST_F(clCreateImage2DTest, noRet) {
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

TEST_F(clCreateImage2DTest, GivenInvalidContextsWhenImageIsCreatedThenReturnsFail) {
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

TEST_F(clCreateImage3DTest, GivenValidInputsWhenImageIsCreatedThenReturnsSuccess) {
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

TEST_F(clCreateImage3DTest, noRet) {
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

TEST_F(clCreateImage3DTest, GivenInvalidContextsWhenImageIsCreatedThenReturnsFail) {
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
} // namespace ULT
