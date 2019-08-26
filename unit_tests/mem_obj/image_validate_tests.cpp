/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "runtime/helpers/convert_color.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef decltype(&Image::redescribe) RedescribeMethod;

class ImageValidateTest : public testing::TestWithParam<cl_image_desc> {
  public:
    ImageValidateTest() {
        imageFormat = &surfaceFormat.OCLImageFormat;
        imageFormat->image_channel_data_type = CL_UNSIGNED_INT8;
        imageFormat->image_channel_order = CL_RGBA;
    }

  protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    SurfaceFormatInfo surfaceFormat;
    cl_image_format *imageFormat;
    cl_image_desc imageDesc;
};

typedef ImageValidateTest ValidDescriptor;
typedef ImageValidateTest InvalidDescriptor;
typedef ImageValidateTest InvalidSize;

TEST_P(ValidDescriptor, validSizePassedToValidateReturnsSuccess) {
    imageDesc = GetParam();
    retVal = Image::validate(&context, {}, &surfaceFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(InvalidDescriptor, zeroSizePassedToValidateReturnsError) {
    imageDesc = GetParam();
    retVal = Image::validate(&context, {}, &surfaceFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

TEST_P(InvalidSize, invalidSizePassedToValidateReturnsError) {
    imageDesc = GetParam();
    retVal = Image::validate(&context, {}, &surfaceFormat, &imageDesc, nullptr);
    EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal);
}

TEST_P(ValidDescriptor, given3dImageFormatWhenGetSupportedFormatIsCalledThenDontReturnDepthFormats) {
    imageDesc = GetParam();
    uint32_t readOnlyformatCount;
    uint32_t writeOnlyformatCount;
    uint32_t readWriteOnlyformatCount;

    context.getSupportedImageFormats(context.getDevice(0), CL_MEM_READ_ONLY, imageDesc.image_type, 0, nullptr, &readOnlyformatCount);
    context.getSupportedImageFormats(context.getDevice(0), CL_MEM_WRITE_ONLY, imageDesc.image_type, 0, nullptr, &writeOnlyformatCount);
    context.getSupportedImageFormats(context.getDevice(0), CL_MEM_READ_WRITE, imageDesc.image_type, 0, nullptr, &readWriteOnlyformatCount);
    auto readOnlyImgFormats = new cl_image_format[readOnlyformatCount];
    auto writeOnlyImgFormats = new cl_image_format[writeOnlyformatCount];
    auto readWriteOnlyImgFormats = new cl_image_format[readWriteOnlyformatCount];
    context.getSupportedImageFormats(context.getDevice(0), CL_MEM_READ_ONLY, imageDesc.image_type, readOnlyformatCount, readOnlyImgFormats, 0);
    context.getSupportedImageFormats(context.getDevice(0), CL_MEM_WRITE_ONLY, imageDesc.image_type, writeOnlyformatCount, writeOnlyImgFormats, 0);
    context.getSupportedImageFormats(context.getDevice(0), CL_MEM_READ_WRITE, imageDesc.image_type, readWriteOnlyformatCount, readWriteOnlyImgFormats, 0);

    bool depthFound = false;
    for (uint32_t i = 0; i < readOnlyformatCount; i++) {
        if (readOnlyImgFormats[i].image_channel_order == CL_DEPTH || readOnlyImgFormats[i].image_channel_order == CL_DEPTH_STENCIL)
            depthFound = true;
    }
    for (uint32_t i = 0; i < readOnlyformatCount; i++) {
        if (readOnlyImgFormats[i].image_channel_order == CL_DEPTH || readOnlyImgFormats[i].image_channel_order == CL_DEPTH_STENCIL)
            depthFound = true;
    }
    for (uint32_t i = 0; i < readOnlyformatCount; i++) {
        if (readOnlyImgFormats[i].image_channel_order == CL_DEPTH || readOnlyImgFormats[i].image_channel_order == CL_DEPTH_STENCIL)
            depthFound = true;
    }

    if (!Image::isImage2dOr2dArray(imageDesc.image_type)) {
        EXPECT_FALSE(depthFound);
    } else {
        EXPECT_TRUE(depthFound);
    }

    delete[] readOnlyImgFormats;
    delete[] writeOnlyImgFormats;
    delete[] readWriteOnlyImgFormats;
}

TEST(ImageDepthFormatTest, returnSurfaceFormatForDepthFormats) {
    cl_image_format imgFormat = {};
    imgFormat.image_channel_order = CL_DEPTH;
    imgFormat.image_channel_data_type = CL_FLOAT;

    auto surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->GMMSurfaceFormat == GMM_FORMAT_R32_FLOAT_TYPE);

    imgFormat.image_channel_data_type = CL_UNORM_INT16;
    surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->GMMSurfaceFormat == GMM_FORMAT_R16_UNORM_TYPE);
}

TEST(ImageDepthFormatTest, returnSurfaceFormatForWriteOnlyDepthFormats) {
    cl_image_format imgFormat = {};
    imgFormat.image_channel_order = CL_DEPTH;
    imgFormat.image_channel_data_type = CL_FLOAT;

    auto surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_WRITE_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->GMMSurfaceFormat == GMM_FORMAT_R32_FLOAT_TYPE);

    imgFormat.image_channel_data_type = CL_UNORM_INT16;
    surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_WRITE_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->GMMSurfaceFormat == GMM_FORMAT_R16_UNORM_TYPE);
}

TEST(ImageDepthFormatTest, returnSurfaceFormatForDepthStencilFormats) {
    cl_image_format imgFormat = {};
    imgFormat.image_channel_order = CL_DEPTH_STENCIL;
    imgFormat.image_channel_data_type = CL_UNORM_INT24;

    auto surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->GMMSurfaceFormat == GMM_FORMAT_GENERIC_32BIT);

    imgFormat.image_channel_order = CL_DEPTH_STENCIL;
    imgFormat.image_channel_data_type = CL_FLOAT;
    surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->GMMSurfaceFormat == GMM_FORMAT_R32G32_FLOAT_TYPE);
}

static cl_image_desc validImageDesc[] = {
    {CL_MEM_OBJECT_IMAGE1D, /*image_type*/
     16384,                 /*image_width*/
     1,                     /*image_height*/
     1,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */

    {CL_MEM_OBJECT_IMAGE1D_ARRAY, /*image_type*/
     16384,                       /*image_width*/
     1,                           /*image_height*/
     1,                           /*image_depth*/
     2,                           /*image_array_size*/
     0,                           /*image_row_pitch*/
     0,                           /*image_slice_pitch*/
     0,                           /*num_mip_levels*/
     0,                           /*num_samples*/
     {0}},                        /*mem_object */

    {CL_MEM_OBJECT_IMAGE2D, /*image_type*/
     512,                   /*image_width*/
     512,                   /*image_height*/
     1,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */

    {CL_MEM_OBJECT_IMAGE2D, /*image_type*/
     16384,                 /*image_width*/
     16384,                 /*image_height*/
     1,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */

    {CL_MEM_OBJECT_IMAGE2D_ARRAY, /*image_type*/
     16384,                       /*image_width*/
     16384,                       /*image_height*/
     1,                           /*image_depth*/
     1,                           /*image_array_size*/
     0,                           /*image_row_pitch*/
     0,                           /*image_slice_pitch*/
     0,                           /*num_mip_levels*/
     0,                           /*num_samples*/
     {0}},                        /*mem_object */

    {CL_MEM_OBJECT_IMAGE2D_ARRAY, /*image_type*/
     16384,                       /*image_width*/
     16384,                       /*image_height*/
     1,                           /*image_depth*/
     2,                           /*image_array_size*/
     0,                           /*image_row_pitch*/
     0,                           /*image_slice_pitch*/
     0,                           /*num_mip_levels*/
     0,                           /*num_samples*/
     {0}},                        /*mem_object */

    {CL_MEM_OBJECT_IMAGE3D, /*image_type*/
     16384,                 /*image_width*/
     16384,                 /*image_height*/
     3,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */

    {CL_MEM_OBJECT_IMAGE3D, /*image_type*/
     2,                     /*image_width*/
     2,                     /*image_height*/
     2,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */
};

static cl_image_desc invalidImageDesc[] = {

    {CL_MEM_OBJECT_IMAGE2D, /*image_type*/
     0,                     /*image_width*/
     512,                   /*image_height*/
     1,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */

    {CL_MEM_OBJECT_IMAGE2D, /*image_type*/
     512,                   /*image_width*/
     0,                     /*image_height*/
     1,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */
};

static cl_image_desc invalidImageSize[] = {
    {CL_MEM_OBJECT_IMAGE2D, /*image_type*/
     16384 + 10,            /*image_width*/
     512,                   /*image_height*/
     1,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */

    {CL_MEM_OBJECT_IMAGE2D, /*image_type*/
     1,                     /*image_width*/
     16384 + 10,            /*image_height*/
     1,                     /*image_depth*/
     0,                     /*image_array_size*/
     0,                     /*image_row_pitch*/
     0,                     /*image_slice_pitch*/
     0,                     /*num_mip_levels*/
     0,                     /*num_samples*/
     {0}},                  /*mem_object */
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidDescriptor,
    ::testing::ValuesIn(validImageDesc));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidDescriptor,
    ::testing::ValuesIn(invalidImageDesc));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidSize,
    ::testing::ValuesIn(invalidImageSize));

class ValidImageFormatTest : public ::testing::TestWithParam<std::tuple<unsigned int, unsigned int>> {
  public:
    void validateFormat() {
        cl_image_format imageFormat;
        cl_int retVal;
        std::tie(imageFormat.image_channel_order, imageFormat.image_channel_data_type) = GetParam();
        retVal = Image::validateImageFormat(&imageFormat);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
};

class InvalidImageFormatTest : public ::testing::TestWithParam<std::tuple<unsigned int, unsigned int>> {
  public:
    void validateFormat() {
        cl_image_format imageFormat;
        cl_int retVal;
        std::tie(imageFormat.image_channel_order, imageFormat.image_channel_data_type) = GetParam();
        retVal = Image::validateImageFormat(&imageFormat);
        EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
    }
};

typedef ValidImageFormatTest ValidSingleChannelFormat;
typedef InvalidImageFormatTest InvalidSingleChannelFormat;

cl_channel_order validSingleChannelOrder[] = {CL_R, CL_A, CL_Rx};
cl_channel_type validSingleChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                 CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT};

cl_channel_type invalidSingleChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidSingleChannelFormat, givenValidSingleChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidSingleChannelFormat, givenInvalidSingleChannelChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidSingleChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validSingleChannelOrder),
        ::testing::ValuesIn(validSingleChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidSingleChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validSingleChannelOrder),
        ::testing::ValuesIn(invalidSingleChannelDataTypes)));

typedef ValidImageFormatTest ValidIntensityFormat;
typedef InvalidImageFormatTest InvalidIntensityFormat;

cl_channel_order validIntensityChannelOrders[] = {CL_INTENSITY};
cl_channel_type validIntensityChannelDataTypes[] = {CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16, CL_HALF_FLOAT, CL_FLOAT};
cl_channel_type invalidIntensityChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                      CL_UNSIGNED_INT32, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidIntensityFormat, givenValidIntensityImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidIntensityFormat, givenInvalidIntensityChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidIntensityFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validIntensityChannelOrders),
        ::testing::ValuesIn(validIntensityChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidIntensityFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validIntensityChannelOrders),
        ::testing::ValuesIn(invalidIntensityChannelDataTypes)));

typedef ValidImageFormatTest ValidLuminanceFormat;
typedef InvalidImageFormatTest InvalidLuminanceFormat;

cl_channel_order validLuminanceChannelOrders[] = {CL_LUMINANCE};
cl_channel_type validLuminanceChannelDataTypes[] = {CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16, CL_HALF_FLOAT, CL_FLOAT};
cl_channel_type invalidLuminanceChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                      CL_UNSIGNED_INT32, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidLuminanceFormat, givenValidLuminanceImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidLuminanceFormat, givenInvalidLuminanceChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidLuminanceFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validLuminanceChannelOrders),
        ::testing::ValuesIn(validLuminanceChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidLuminanceFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validLuminanceChannelOrders),
        ::testing::ValuesIn(invalidLuminanceChannelDataTypes)));

typedef ValidImageFormatTest ValidDepthFormat;
typedef InvalidImageFormatTest InvalidDepthFormat;

cl_channel_order validDepthChannelOrders[] = {CL_DEPTH};
cl_channel_type validDepthChannelDataTypes[] = {CL_UNORM_INT16, CL_FLOAT};
cl_channel_type invalidDepthChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                  CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidDepthFormat, givenValidDepthImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidDepthFormat, givenInvalidDepthChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidDepthFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validDepthChannelOrders),
        ::testing::ValuesIn(validDepthChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidDepthFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validDepthChannelOrders),
        ::testing::ValuesIn(invalidDepthChannelDataTypes)));

typedef ValidImageFormatTest ValidDoubleChannelFormat;
typedef InvalidImageFormatTest InvalidDoubleChannelFormat;

cl_channel_order validDoubleChannelOrders[] = {CL_RG, CL_RGx, CL_RA};
cl_channel_type validDoubleChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                 CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT};
cl_channel_type invalidDoubleChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidDoubleChannelFormat, givenValidDoubleChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidDoubleChannelFormat, givenInvalidDoubleChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidDoubleChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validDoubleChannelOrders),
        ::testing::ValuesIn(validDoubleChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidDoubleChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validDoubleChannelOrders),
        ::testing::ValuesIn(invalidDoubleChannelDataTypes)));

typedef ValidImageFormatTest ValidTripleChannelFormat;
typedef InvalidImageFormatTest InvalidTripleChannelFormat;

cl_channel_order validTripleChannelOrders[] = {CL_RGB, CL_RGBx};
cl_channel_type validTripleChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010};
cl_channel_type invalidTripleChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                   CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidTripleChannelFormat, givenValidTripleChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidTripleChannelFormat, givenInvalidTripleChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidTripleChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validTripleChannelOrders),
        ::testing::ValuesIn(validTripleChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidTripleChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validTripleChannelOrders),
        ::testing::ValuesIn(invalidTripleChannelDataTypes)));

typedef ValidImageFormatTest ValidRGBAChannelFormat;
typedef InvalidImageFormatTest InvalidRGBAChannelFormat;

cl_channel_order validRGBAChannelOrders[] = {CL_RGBA};
cl_channel_type validRGBAChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                               CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT};
cl_channel_type invalidRGBAChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidRGBAChannelFormat, givenValidRGBAChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidRGBAChannelFormat, givenInvalidRGBAChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidRGBAChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validRGBAChannelOrders),
        ::testing::ValuesIn(validRGBAChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidRGBAChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validRGBAChannelOrders),
        ::testing::ValuesIn(invalidRGBAChannelDataTypes)));

typedef ValidImageFormatTest ValidSRGBChannelFormat;
typedef InvalidImageFormatTest InvalidSRGBChannelFormat;

cl_channel_order validSRGBChannelOrders[] = {CL_sRGB, CL_sRGBx, CL_sRGBA, CL_sBGRA};
cl_channel_type validSRGBChannelDataTypes[] = {CL_UNORM_INT8};
cl_channel_type invalidSRGBChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                 CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidSRGBChannelFormat, givenValidSRGBChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidSRGBChannelFormat, givenInvalidSRGBChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidSRGBChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validSRGBChannelOrders),
        ::testing::ValuesIn(validSRGBChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidSRGBChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validSRGBChannelOrders),
        ::testing::ValuesIn(invalidSRGBChannelDataTypes)));

typedef ValidImageFormatTest ValidARGBChannelFormat;
typedef InvalidImageFormatTest InvalidARGBChannelFormat;

cl_channel_order validARGBChannelOrders[] = {CL_ARGB, CL_BGRA, CL_ABGR};
cl_channel_type validARGBChannelDataTypes[] = {CL_UNORM_INT8, CL_SNORM_INT8, CL_SIGNED_INT8, CL_UNSIGNED_INT8};
cl_channel_type invalidARGBChannelDataTypes[] = {CL_SNORM_INT16, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT16,
                                                 CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidARGBChannelFormat, givenValidARGBChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidARGBChannelFormat, givenInvalidARGBChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidARGBChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validARGBChannelOrders),
        ::testing::ValuesIn(validARGBChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidARGBChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validARGBChannelOrders),
        ::testing::ValuesIn(invalidARGBChannelDataTypes)));

typedef ValidImageFormatTest ValidDepthStencilChannelFormat;
typedef InvalidImageFormatTest InvalidDepthStencilChannelFormat;

cl_channel_order validDepthStencilChannelOrders[] = {CL_DEPTH_STENCIL};
cl_channel_type validDepthStencilChannelDataTypes[] = {CL_UNORM_INT24, CL_FLOAT};
cl_channel_type invalidDepthStencilChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                         CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_UNORM_INT_101010_2};

TEST_P(ValidDepthStencilChannelFormat, givenValidDepthStencilChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidDepthStencilChannelFormat, givenInvalidDepthStencilChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidDepthStencilChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validDepthStencilChannelOrders),
        ::testing::ValuesIn(validDepthStencilChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidDepthStencilChannelFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validDepthStencilChannelOrders),
        ::testing::ValuesIn(invalidDepthStencilChannelDataTypes)));

typedef ValidImageFormatTest ValidYUVImageFormat;
typedef InvalidImageFormatTest InvalidYUVImageFormat;

cl_channel_order validYUVChannelOrders[] = {CL_NV12_INTEL, CL_YUYV_INTEL, CL_UYVY_INTEL, CL_YVYU_INTEL, CL_VYUY_INTEL};
cl_channel_type validYUVChannelDataTypes[] = {CL_UNORM_INT8};
cl_channel_type invalidYUVChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST_P(ValidYUVImageFormat, givenValidYUVImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    validateFormat();
};

TEST_P(InvalidYUVImageFormat, givenInvalidYUVChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    validateFormat();
};

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    ValidYUVImageFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validYUVChannelOrders),
        ::testing::ValuesIn(validYUVChannelDataTypes)));

INSTANTIATE_TEST_CASE_P(
    ImageValidate,
    InvalidYUVImageFormat,
    ::testing::Combine(
        ::testing::ValuesIn(validYUVChannelOrders),
        ::testing::ValuesIn(invalidYUVChannelDataTypes)));

TEST(ImageFormat, givenNullptrImageFormatWhenValidateImageFormatIsCalledThenReturnsError) {
    auto retVal = Image::validateImageFormat(nullptr);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST(validateAndCreateImage, givenInvalidImageFormatWhenValidateAndCreateImageIsCalledThenReturnsInvalidDescriptorError) {
    MockContext context;
    cl_image_format imageFormat;
    cl_int retVal = CL_SUCCESS;
    Image *image;
    imageFormat.image_channel_order = 0;
    imageFormat.image_channel_data_type = 0;
    image = Image::validateAndCreateImage(&context, 0, &imageFormat, &Image1dDefaults::imageDesc, nullptr, retVal);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST(validateAndCreateImage, givenNotSupportedImageFormatWhenValidateAndCreateImageIsCalledThenReturnsNotSupportedFormatError) {
    MockContext context;
    cl_image_format imageFormat = {CL_INTENSITY, CL_UNORM_INT8};
    cl_int retVal = CL_SUCCESS;
    Image *image;
    image = Image::validateAndCreateImage(&context, CL_MEM_READ_WRITE, &imageFormat, &Image1dDefaults::imageDesc, nullptr, retVal);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal);
}

TEST(validateAndCreateImage, givenValidImageParamsWhenValidateAndCreateImageIsCalledThenReturnsSuccess) {
    MockContext context;
    cl_image_desc imageDesc;
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 1D image with 0 row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 10;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;

    cl_image_format imageFormat = {CL_INTENSITY, CL_UNORM_INT8};
    cl_int retVal = CL_SUCCESS;

    std::unique_ptr<Image> image = nullptr;
    image.reset(Image::validateAndCreateImage(
        &context,
        flags,
        &imageFormat,
        &imageDesc,
        nullptr,
        retVal));
    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

std::tuple<uint32_t, int32_t> normalizingFactorValues[] = {
    std::make_tuple(CL_SNORM_INT8, 0x7F),
    std::make_tuple(CL_SNORM_INT16, 0x7fFF),
    std::make_tuple(CL_UNORM_INT8, 0xFF),
    std::make_tuple(CL_UNORM_INT16, 0xFFFF),
    std::make_tuple(CL_UNORM_SHORT_565, 0),
    std::make_tuple(CL_UNORM_SHORT_555, 0),
    std::make_tuple(CL_UNORM_INT_101010, 0),
    std::make_tuple(CL_SIGNED_INT8, 0),
    std::make_tuple(CL_SIGNED_INT16, 0),
    std::make_tuple(CL_SIGNED_INT32, 0),
    std::make_tuple(CL_UNSIGNED_INT8, 0),
    std::make_tuple(CL_UNSIGNED_INT16, 0),
    std::make_tuple(CL_UNSIGNED_INT32, 0),
    std::make_tuple(CL_HALF_FLOAT, 0),
    std::make_tuple(CL_FLOAT, 0),
    std::make_tuple(CL_UNORM_INT24, 0),
    std::make_tuple(CL_UNORM_INT_101010_2, 0),
};

using NormalizingFactorTests = ::testing::TestWithParam<std::tuple<uint32_t, int32_t>>;

TEST_P(NormalizingFactorTests, givenChannelTypeWhenAskingForFactorThenReturnValidValue) {
    auto factor = selectNormalizingFactor(std::get<0>(GetParam()));
    EXPECT_EQ(std::get<1>(GetParam()), factor);
};

INSTANTIATE_TEST_CASE_P(
    NormalizingFactorTests,
    NormalizingFactorTests,
    ::testing::ValuesIn(normalizingFactorValues));

using ValidParentImageFormatTest = ::testing::TestWithParam<std::tuple<uint32_t, uint32_t>>;

cl_channel_order allChannelOrders[] = {CL_R, CL_A, CL_RG, CL_RA, CL_RGB, CL_RGBA, CL_BGRA, CL_ARGB, CL_INTENSITY, CL_LUMINANCE, CL_Rx, CL_RGx, CL_RGBx, CL_DEPTH, CL_DEPTH_STENCIL, CL_sRGB,
                                       CL_sRGBx, CL_sRGBA, CL_sBGRA, CL_ABGR, CL_NV12_INTEL};

struct NullImage : public Image {
    using Image::imageDesc;
    using Image::imageFormat;

    NullImage() : Image(nullptr, cl_mem_flags{}, 0, nullptr, cl_image_format{},
                        cl_image_desc{}, false, new MockGraphicsAllocation(nullptr, 0), false,
                        0, 0, SurfaceFormatInfo{}, nullptr) {
    }
    ~NullImage() override {
        delete this->graphicsAllocation;
    }
    void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel) override {}
    void setMediaImageArg(void *memory) override {}
    void setMediaSurfaceRotation(void *memory) override {}
    void setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) override {}
    void transformImage2dArrayTo3d(void *memory) override {}
    void transformImage3dTo2dArray(void *memory) override {}
};

TEST_P(ValidParentImageFormatTest, givenParentChannelOrderWhenTestWithAllChannelOrdersThenReturnTrueForValidChannelOrder) {
    cl_image_format parentImageFormat;
    cl_image_format imageFormat;
    cl_channel_order validChannelOrder;
    NullImage image;
    std::tie(parentImageFormat.image_channel_order, validChannelOrder) = GetParam();
    parentImageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    image.imageFormat = parentImageFormat;

    for (auto channelOrder : allChannelOrders) {
        imageFormat.image_channel_order = channelOrder;
        bool retVal = image.hasValidParentImageFormat(imageFormat);
        EXPECT_EQ(imageFormat.image_channel_order == validChannelOrder, retVal);
    }
};
std::tuple<uint32_t, uint32_t> imageFromImageValidChannelOrderPairs[] = {
    std::make_tuple(CL_BGRA, CL_sBGRA),
    std::make_tuple(CL_sBGRA, CL_BGRA),
    std::make_tuple(CL_RGBA, CL_sRGBA),
    std::make_tuple(CL_sRGBA, CL_RGBA),
    std::make_tuple(CL_RGB, CL_sRGB),
    std::make_tuple(CL_sRGB, CL_RGB),
    std::make_tuple(CL_RGBx, CL_sRGBx),
    std::make_tuple(CL_sRGBx, CL_RGBx),
    std::make_tuple(CL_R, CL_DEPTH),
    std::make_tuple(CL_A, 0),
    std::make_tuple(CL_RG, 0),
    std::make_tuple(CL_RA, 0),
    std::make_tuple(CL_ARGB, 0),
    std::make_tuple(CL_INTENSITY, 0),
    std::make_tuple(CL_LUMINANCE, 0),
    std::make_tuple(CL_Rx, 0),
    std::make_tuple(CL_RGx, 0),
    std::make_tuple(CL_DEPTH, 0),
    std::make_tuple(CL_DEPTH_STENCIL, 0),
    std::make_tuple(CL_ABGR, 0),
    std::make_tuple(CL_NV12_INTEL, 0)};

INSTANTIATE_TEST_CASE_P(
    ValidParentImageFormatTests,
    ValidParentImageFormatTest,
    ::testing::ValuesIn(imageFromImageValidChannelOrderPairs));

TEST(ImageDescriptorComparatorTest, givenImageWhenCallHasSameDescriptorWithSameDescriptorThenReturnTrueOtherwiseFalse) {
    NullImage image;
    cl_image_desc descriptor = image.imageDesc;
    image.imageDesc.image_row_pitch = image.getHostPtrRowPitch() + 10; // to make sure we compare host ptr row/slice pitches
    image.imageDesc.image_slice_pitch = image.getHostPtrSlicePitch() + 10;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.image_type++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.image_type--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.image_width++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.image_width--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.image_height++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.image_height--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.image_depth++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.image_depth--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.image_array_size++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.image_array_size--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.image_row_pitch++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.image_row_pitch--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.image_slice_pitch++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.image_slice_pitch--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.num_mip_levels++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
    descriptor.num_mip_levels--;
    EXPECT_TRUE(image.hasSameDescriptor(descriptor));
    descriptor.num_samples++;
    EXPECT_FALSE(image.hasSameDescriptor(descriptor));
};

TEST(ImageFormatValidatorTest, givenValidParentChannelOrderAndChannelOrderWhenFormatsHaveDifferentDataTypeThenHasValidParentImageFormatReturnsFalse) {
    cl_image_format imageFormat;
    NullImage image;
    image.imageFormat.image_channel_data_type = CL_UNORM_INT8;
    image.imageFormat.image_channel_order = CL_BGRA;
    imageFormat.image_channel_data_type = CL_UNORM_INT16;
    imageFormat.image_channel_order = CL_sBGRA;
    EXPECT_FALSE(image.hasValidParentImageFormat(imageFormat));
};
TEST(ImageValidatorTest, givenInvalidImage2dSizesWithoutParentObjectWhenValidateImageThenReturnsError) {
    MockContext context;
    cl_image_desc descriptor;
    void *dummyPtr = reinterpret_cast<void *>(0x17);
    SurfaceFormatInfo surfaceFormat;
    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_row_pitch = 0;

    descriptor.image_height = 1;
    descriptor.image_width = 0;
    descriptor.mem_object = nullptr;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));

    descriptor.image_height = 0;
    descriptor.image_width = 1;
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
TEST(ImageValidatorTest, givenNV12Image2dAsParentImageWhenValidateImageZeroSizedThenReturnsSuccess) {
    NullImage image;
    cl_image_desc descriptor;
    MockContext context;
    void *dummyPtr = reinterpret_cast<void *>(0x17);
    SurfaceFormatInfo surfaceFormat = {};
    image.imageFormat.image_channel_order = CL_NV12_INTEL;

    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_height = 0;
    descriptor.image_width = 0;
    descriptor.image_row_pitch = 0;
    descriptor.mem_object = &image;

    EXPECT_EQ(CL_SUCCESS, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
TEST(ImageValidatorTest, givenNonNV12Image2dAsParentImageWhenValidateImageZeroSizedThenReturnsError) {
    NullImage image;
    cl_image_desc descriptor;
    MockContext context;
    void *dummyPtr = reinterpret_cast<void *>(0x17);
    SurfaceFormatInfo surfaceFormat;
    image.imageFormat.image_channel_order = CL_BGRA;
    image.imageFormat.image_channel_data_type = CL_UNORM_INT8;

    surfaceFormat.OCLImageFormat.image_channel_order = CL_sBGRA;
    surfaceFormat.OCLImageFormat.image_channel_data_type = CL_UNORM_INT8;
    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_height = 0;
    descriptor.image_width = 0;
    descriptor.image_row_pitch = image.getHostPtrRowPitch();
    descriptor.image_slice_pitch = image.getHostPtrSlicePitch();
    image.imageDesc = descriptor;
    descriptor.mem_object = &image;

    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
