/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/convert_color.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef decltype(&Image::redescribe) RedescribeMethod;

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

class ImageValidateTest : public ::testing::Test {
  public:
    ImageValidateTest() {
        imageFormat = &surfaceFormat.oclImageFormat;
        imageFormat->image_channel_data_type = CL_UNSIGNED_INT8;
        imageFormat->image_channel_order = CL_RGBA;
    }

  protected:
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    ClSurfaceFormatInfo surfaceFormat;
    cl_image_format *imageFormat;
    cl_image_desc imageDesc;
};

TEST_F(ImageValidateTest, GivenValidSizeWhenValidatingThenSuccessIsReturned) {
    for (const auto &desc : validImageDesc) {
        imageDesc = desc;
        retVal = Image::validate(&context, {}, &surfaceFormat, &imageDesc, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal) << " image_type=" << desc.image_type;
    }
}

TEST_F(ImageValidateTest, GivenZeroSizeWhenValidatingThenInvalidImageDescriptorErrorIsReturned) {
    for (const auto &desc : invalidImageDesc) {
        imageDesc = desc;
        retVal = Image::validate(&context, {}, &surfaceFormat, &imageDesc, nullptr);
        EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal) << " image_type=" << desc.image_type;
    }
}

TEST_F(ImageValidateTest, GivenInvalidSizeWhenValidatingThenInvalidImageSizeErrorIsReturned) {
    for (const auto &desc : invalidImageSize) {
        imageDesc = desc;
        retVal = Image::validate(&context, {}, &surfaceFormat, &imageDesc, nullptr);
        EXPECT_EQ(CL_INVALID_IMAGE_SIZE, retVal) << " image_type=" << desc.image_type;
    }
}

TEST_F(ImageValidateTest, given3dImageFormatWhenGetSupportedFormatIsCalledThenDontReturnDepthFormats) {
    for (const auto &desc : validImageDesc) {
        imageDesc = desc;
        uint32_t readOnlyformatCount;
        uint32_t writeOnlyformatCount;
        uint32_t readWriteOnlyformatCount;

        context.getSupportedImageFormats(&context.getDevice(0)->getDevice(), CL_MEM_READ_ONLY, imageDesc.image_type, 0, nullptr, &readOnlyformatCount);
        context.getSupportedImageFormats(&context.getDevice(0)->getDevice(), CL_MEM_WRITE_ONLY, imageDesc.image_type, 0, nullptr, &writeOnlyformatCount);
        context.getSupportedImageFormats(&context.getDevice(0)->getDevice(), CL_MEM_READ_WRITE, imageDesc.image_type, 0, nullptr, &readWriteOnlyformatCount);
        auto readOnlyImgFormats = new cl_image_format[readOnlyformatCount];
        auto writeOnlyImgFormats = new cl_image_format[writeOnlyformatCount];
        auto readWriteOnlyImgFormats = new cl_image_format[readWriteOnlyformatCount];
        context.getSupportedImageFormats(&context.getDevice(0)->getDevice(), CL_MEM_READ_ONLY, imageDesc.image_type, readOnlyformatCount, readOnlyImgFormats, 0);
        context.getSupportedImageFormats(&context.getDevice(0)->getDevice(), CL_MEM_WRITE_ONLY, imageDesc.image_type, writeOnlyformatCount, writeOnlyImgFormats, 0);
        context.getSupportedImageFormats(&context.getDevice(0)->getDevice(), CL_MEM_READ_WRITE, imageDesc.image_type, readWriteOnlyformatCount, readWriteOnlyImgFormats, 0);

        bool depthFound = false;
        for (uint32_t i = 0; i < readOnlyformatCount; i++) {
            if (readOnlyImgFormats[i].image_channel_order == CL_DEPTH || readOnlyImgFormats[i].image_channel_order == CL_DEPTH_STENCIL) {
                depthFound = true;
            }
        }
        for (uint32_t i = 0; i < readOnlyformatCount; i++) {
            if (readOnlyImgFormats[i].image_channel_order == CL_DEPTH || readOnlyImgFormats[i].image_channel_order == CL_DEPTH_STENCIL) {
                depthFound = true;
            }
        }
        for (uint32_t i = 0; i < readOnlyformatCount; i++) {
            if (readOnlyImgFormats[i].image_channel_order == CL_DEPTH || readOnlyImgFormats[i].image_channel_order == CL_DEPTH_STENCIL) {
                depthFound = true;
            }
        }

        if (!Image::isImage2dOr2dArray(imageDesc.image_type)) {
            EXPECT_FALSE(depthFound) << " image_type=" << desc.image_type;
        } else {
            EXPECT_TRUE(depthFound) << " image_type=" << desc.image_type;
        }

        delete[] readOnlyImgFormats;
        delete[] writeOnlyImgFormats;
        delete[] readWriteOnlyImgFormats;
    }
}

TEST(ImageDepthFormatTest, GivenDepthFormatsWhenGettingSurfaceFormatThenCorrectSurfaceFormatIsReturned) {
    cl_image_format imgFormat = {};
    imgFormat.image_channel_order = CL_DEPTH;
    imgFormat.image_channel_data_type = CL_FLOAT;

    auto surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->surfaceFormat.gmmSurfaceFormat == GMM_FORMAT_R32_FLOAT_TYPE);

    imgFormat.image_channel_data_type = CL_UNORM_INT16;
    surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->surfaceFormat.gmmSurfaceFormat == GMM_FORMAT_R16_UNORM_TYPE);
}

TEST(ImageDepthFormatTest, GivenWriteOnlyDepthFormatsWhenGettingSurfaceFormatThenCorrectSurfaceFormatIsReturned) {
    cl_image_format imgFormat = {};
    imgFormat.image_channel_order = CL_DEPTH;
    imgFormat.image_channel_data_type = CL_FLOAT;

    auto surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_WRITE_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->surfaceFormat.gmmSurfaceFormat == GMM_FORMAT_R32_FLOAT_TYPE);

    imgFormat.image_channel_data_type = CL_UNORM_INT16;
    surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_WRITE_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->surfaceFormat.gmmSurfaceFormat == GMM_FORMAT_R16_UNORM_TYPE);
}

TEST(ImageDepthFormatTest, GivenDepthStencilFormatsWhenGettingSurfaceFormatThenCorrectSurfaceFormatIsReturned) {
    cl_image_format imgFormat = {};
    imgFormat.image_channel_order = CL_DEPTH_STENCIL;
    imgFormat.image_channel_data_type = CL_UNORM_INT24;

    auto surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->surfaceFormat.gmmSurfaceFormat == GMM_FORMAT_GENERIC_32BIT);

    imgFormat.image_channel_order = CL_DEPTH_STENCIL;
    imgFormat.image_channel_data_type = CL_FLOAT;
    surfaceFormatInfo = Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &imgFormat);
    ASSERT_NE(surfaceFormatInfo, nullptr);
    EXPECT_TRUE(surfaceFormatInfo->surfaceFormat.gmmSurfaceFormat == GMM_FORMAT_R32G32_FLOAT_TYPE);
}

static cl_channel_order validSingleChannelOrder[] = {CL_R, CL_A, CL_Rx};
static cl_channel_type validSingleChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                        CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT};
static cl_channel_type invalidSingleChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidSingleChannelFormat, givenValidSingleChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validSingleChannelOrder) {
        for (auto dataType : validSingleChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidSingleChannelFormat, givenInvalidSingleChannelChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validSingleChannelOrder) {
        for (auto dataType : invalidSingleChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validIntensityChannelOrders[] = {CL_INTENSITY};
static cl_channel_type validIntensityChannelDataTypes[] = {CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16, CL_HALF_FLOAT, CL_FLOAT};
static cl_channel_type invalidIntensityChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                             CL_UNSIGNED_INT32, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidIntensityFormat, givenValidIntensityImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validIntensityChannelOrders) {
        for (auto dataType : validIntensityChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidIntensityFormat, givenInvalidIntensityChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validIntensityChannelOrders) {
        for (auto dataType : invalidIntensityChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validLuminanceChannelOrders[] = {CL_LUMINANCE};
static cl_channel_type validLuminanceChannelDataTypes[] = {CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16, CL_HALF_FLOAT, CL_FLOAT};
static cl_channel_type invalidLuminanceChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                             CL_UNSIGNED_INT32, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidLuminanceFormat, givenValidLuminanceImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validLuminanceChannelOrders) {
        for (auto dataType : validLuminanceChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidLuminanceFormat, givenInvalidLuminanceChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validLuminanceChannelOrders) {
        for (auto dataType : invalidLuminanceChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validDepthChannelOrders[] = {CL_DEPTH};
static cl_channel_type validDepthChannelDataTypes[] = {CL_UNORM_INT16, CL_FLOAT};
static cl_channel_type invalidDepthChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                         CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidDepthFormat, givenValidDepthImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validDepthChannelOrders) {
        for (auto dataType : validDepthChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidDepthFormat, givenInvalidDepthChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validDepthChannelOrders) {
        for (auto dataType : invalidDepthChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validDoubleChannelOrders[] = {CL_RG, CL_RGx, CL_RA};
static cl_channel_type validDoubleChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                        CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT};
static cl_channel_type invalidDoubleChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidDoubleChannelFormat, givenValidDoubleChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validDoubleChannelOrders) {
        for (auto dataType : validDoubleChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidDoubleChannelFormat, givenInvalidDoubleChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validDoubleChannelOrders) {
        for (auto dataType : invalidDoubleChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validTripleChannelOrders[] = {CL_RGB, CL_RGBx};
static cl_channel_type validTripleChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010};
static cl_channel_type invalidTripleChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                          CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidTripleChannelFormat, givenValidTripleChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validTripleChannelOrders) {
        for (auto dataType : validTripleChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidTripleChannelFormat, givenInvalidTripleChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validTripleChannelOrders) {
        for (auto dataType : invalidTripleChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validRGBAChannelOrders[] = {CL_RGBA};
static cl_channel_type validRGBAChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                      CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT};
static cl_channel_type invalidRGBAChannelDataTypes[] = {CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidRGBAChannelFormat, givenValidRGBAChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validRGBAChannelOrders) {
        for (auto dataType : validRGBAChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidRGBAChannelFormat, givenInvalidRGBAChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validRGBAChannelOrders) {
        for (auto dataType : invalidRGBAChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validSRGBChannelOrders[] = {CL_sRGB, CL_sRGBx, CL_sRGBA, CL_sBGRA};
static cl_channel_type validSRGBChannelDataTypes[] = {CL_UNORM_INT8};
static cl_channel_type invalidSRGBChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                        CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidSRGBChannelFormat, givenValidSRGBChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validSRGBChannelOrders) {
        for (auto dataType : validSRGBChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidSRGBChannelFormat, givenInvalidSRGBChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validSRGBChannelOrders) {
        for (auto dataType : invalidSRGBChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validARGBChannelOrders[] = {CL_ARGB, CL_BGRA, CL_ABGR};
static cl_channel_type validARGBChannelDataTypes[] = {CL_UNORM_INT8, CL_SNORM_INT8, CL_SIGNED_INT8, CL_UNSIGNED_INT8};
static cl_channel_type invalidARGBChannelDataTypes[] = {CL_SNORM_INT16, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT16,
                                                        CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidARGBChannelFormat, givenValidARGBChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validARGBChannelOrders) {
        for (auto dataType : validARGBChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidARGBChannelFormat, givenInvalidARGBChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validARGBChannelOrders) {
        for (auto dataType : invalidARGBChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validDepthStencilChannelOrders[] = {CL_DEPTH_STENCIL};
static cl_channel_type validDepthStencilChannelDataTypes[] = {CL_UNORM_INT24, CL_FLOAT};
static cl_channel_type invalidDepthStencilChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                                CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_UNORM_INT_101010_2};

TEST(ValidDepthStencilChannelFormat, givenValidDepthStencilChannelImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validDepthStencilChannelOrders) {
        for (auto dataType : validDepthStencilChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidDepthStencilChannelFormat, givenInvalidDepthStencilChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validDepthStencilChannelOrders) {
        for (auto dataType : invalidDepthStencilChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

static cl_channel_order validYUVChannelOrders[] = {CL_NV12_INTEL, CL_YUYV_INTEL, CL_UYVY_INTEL, CL_YVYU_INTEL, CL_VYUY_INTEL};
static cl_channel_type validYUVChannelDataTypes[] = {CL_UNORM_INT8};
static cl_channel_type invalidYUVChannelDataTypes[] = {CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT16, CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32, CL_UNSIGNED_INT8, CL_UNSIGNED_INT16,
                                                       CL_UNSIGNED_INT32, CL_HALF_FLOAT, CL_FLOAT, CL_UNORM_INT24, CL_UNORM_INT_101010_2};

TEST(ValidYUVImageFormat, givenValidYUVImageFormatWhenValidateImageFormatIsCalledThenReturnsSuccess) {
    for (auto order : validYUVChannelOrders) {
        for (auto dataType : validYUVChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_SUCCESS, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(InvalidYUVImageFormat, givenInvalidYUVChannelDataTypeWhenValidateImageFormatIsCalledThenReturnsError) {
    for (auto order : validYUVChannelOrders) {
        for (auto dataType : invalidYUVChannelDataTypes) {
            cl_image_format imageFormat = {order, dataType};
            EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, Image::validateImageFormat(&imageFormat)) << " order=" << order << " dataType=" << dataType;
        }
    }
}

TEST(ImageFormat, givenNullptrImageFormatWhenValidateImageFormatIsCalledThenReturnsError) {
    auto retVal = Image::validateImageFormat(nullptr);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST(validateAndCreateImage, givenInvalidImageFormatWhenValidateAndCreateImageIsCalledThenReturnsInvalidDescriptorError) {
    MockContext context;
    cl_image_format imageFormat;
    cl_int retVal = CL_SUCCESS;
    cl_mem image;
    imageFormat.image_channel_order = 0;
    imageFormat.image_channel_data_type = 0;
    image = Image::validateAndCreateImage(&context, nullptr, 0, 0, &imageFormat, &Image1dDefaults::imageDesc, nullptr, retVal);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST(validateAndCreateImage, givenNotSupportedImageFormatWhenValidateAndCreateImageIsCalledThenReturnsNotSupportedFormatError) {
    MockContext context;
    for (cl_channel_order channelOrder : {CL_INTENSITY, CL_LUMINANCE}) {
        cl_image_format imageFormat = {channelOrder, CL_UNORM_INT8};
        cl_int retVal = CL_SUCCESS;
        cl_mem image;
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        image = Image::validateAndCreateImage(&context, nullptr, flags, 0, &imageFormat, &Image1dDefaults::imageDesc, nullptr, retVal);
        EXPECT_EQ(nullptr, image);
        EXPECT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal);
    }
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
    image.reset(static_cast<Image *>(Image::validateAndCreateImage(
        &context,
        nullptr,
        flags,
        0,
        &imageFormat,
        &imageDesc,
        nullptr,
        retVal)));
    EXPECT_NE(nullptr, image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

static std::tuple<uint32_t, int32_t> normalizingFactorValues[] = {
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

TEST(NormalizingFactorTests, givenChannelTypeWhenAskingForFactorThenReturnValidValue) {
    for (const auto &[channelType, expectedFactor] : normalizingFactorValues) {
        EXPECT_EQ(expectedFactor, selectNormalizingFactor(channelType)) << " channelType=" << channelType;
    }
}

static cl_channel_order allChannelOrders[] = {CL_R, CL_A, CL_RG, CL_RA, CL_RGB, CL_RGBA, CL_BGRA, CL_ARGB, CL_INTENSITY, CL_LUMINANCE, CL_Rx, CL_RGx, CL_RGBx, CL_DEPTH, CL_DEPTH_STENCIL, CL_sRGB,
                                              CL_sRGBx, CL_sRGBA, CL_sBGRA, CL_ABGR, CL_NV12_INTEL, CL_YUYV_INTEL};

struct NullImage : public Image {
    using Image::imageDesc;
    using Image::imageFormat;

    NullImage() : Image(nullptr, MemoryProperties(), cl_mem_flags{}, 0, 0, nullptr, nullptr, cl_image_format{},
                        cl_image_desc{}, false, GraphicsAllocationHelper::toMultiGraphicsAllocation(new MockGraphicsAllocation(nullptr, 0)), false,
                        0, 0, ClSurfaceFormatInfo{}, nullptr) {
    }
    ~NullImage() override {
        delete this->multiGraphicsAllocation.getGraphicsAllocation(0);
    }
    void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel, uint32_t rootDeviceIndex) override {}
};

static std::tuple<uint32_t, uint32_t> imageFromImageValidChannelOrderPairs[] = {
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
    std::make_tuple(CL_NV12_INTEL, 0),
    std::make_tuple(CL_YUYV_INTEL, CL_RGBA)};

TEST(ValidParentImageFormatTest, givenParentChannelOrderWhenTestWithAllChannelOrdersThenReturnTrueForValidChannelOrder) {
    for (const auto &[parentOrder, validChannelOrder] : imageFromImageValidChannelOrderPairs) {
        NullImage image;
        cl_image_format parentImageFormat = {parentOrder, CL_UNORM_INT8};
        cl_image_format imageFormat;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        image.imageFormat = parentImageFormat;
        for (auto channelOrder : allChannelOrders) {
            imageFormat.image_channel_order = channelOrder;
            bool retVal = image.hasValidParentImageFormat(imageFormat);
            EXPECT_EQ(imageFormat.image_channel_order == validChannelOrder, retVal) << " parent=" << parentOrder << " channel=" << channelOrder;
        }
    }
}

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
    ClSurfaceFormatInfo surfaceFormat;
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
TEST(ImageValidatorTest, givenPackedYUVImage2dAsParentImageWhenValidateImageZeroSizedThenReturnsSuccess) {
    NullImage image;
    cl_image_desc descriptor;
    MockContext context;
    REQUIRE_IMAGES_OR_SKIP(&context);

    void *dummyPtr = reinterpret_cast<void *>(0x17);
    ClSurfaceFormatInfo surfaceFormat = {};
    surfaceFormat.oclImageFormat.image_channel_order = CL_RGBA;
    image.imageFormat.image_channel_order = CL_YUYV_INTEL;

    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_height = 0;
    descriptor.image_width = 0;
    descriptor.image_row_pitch = 0;
    descriptor.mem_object = &image;

    EXPECT_EQ(CL_SUCCESS, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
TEST(ImageValidatorTest, givenPackedYUVImage2dAsParentImageWhenValidateImageTwoChannelsThenReturnsFailure) {
    NullImage image;
    cl_image_desc descriptor;
    MockContext context;
    REQUIRE_IMAGES_OR_SKIP(&context);

    void *dummyPtr = reinterpret_cast<void *>(0x17);
    ClSurfaceFormatInfo surfaceFormat = {};
    surfaceFormat.oclImageFormat.image_channel_order = CL_RG;
    image.imageFormat.image_channel_order = CL_YUYV_INTEL;

    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_height = 0;
    descriptor.image_width = 0;
    descriptor.image_row_pitch = 0;
    descriptor.mem_object = &image;

    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
TEST(ImageValidatorTest, givenPackedYUVImage2dAsParentImageWhenValidateImageInvalidSizeThenReturnsFailure) {
    NullImage image;
    cl_image_desc descriptor;
    MockContext context;
    REQUIRE_IMAGES_OR_SKIP(&context);

    void *dummyPtr = reinterpret_cast<void *>(0x17);
    ClSurfaceFormatInfo surfaceFormat = {};
    image.imageDesc.image_width = 4u;
    image.imageDesc.image_height = 4u;
    surfaceFormat.oclImageFormat.image_channel_order = CL_RGBA;
    image.imageFormat.image_channel_order = CL_YUYV_INTEL;

    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_height = 4;
    descriptor.image_width = 1;
    descriptor.image_row_pitch = 0;
    descriptor.mem_object = &image;

    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
TEST(ImageValidatorTest, givenPackedYUVImage2dAsParentImageWhenValidateImageInvalidSizeAndChannelsThenReturnsFailure) {
    NullImage image;
    cl_image_desc descriptor;
    MockContext context;
    REQUIRE_IMAGES_OR_SKIP(&context);

    void *dummyPtr = reinterpret_cast<void *>(0x17);
    ClSurfaceFormatInfo surfaceFormat = {};
    image.imageDesc.image_width = 4u;
    image.imageDesc.image_height = 4u;
    surfaceFormat.oclImageFormat.image_channel_order = CL_RG;
    image.imageFormat.image_channel_order = CL_YUYV_INTEL;

    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_height = 4;
    descriptor.image_width = 1;
    descriptor.image_row_pitch = 0;
    descriptor.mem_object = &image;

    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
TEST(ImageValidatorTest, givenNV12Image2dAsParentImageWhenValidateImageZeroSizedThenReturnsSuccess) {
    NullImage image;
    cl_image_desc descriptor;
    MockContext context;
    REQUIRE_IMAGES_OR_SKIP(&context);

    void *dummyPtr = reinterpret_cast<void *>(0x17);
    ClSurfaceFormatInfo surfaceFormat = {};
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
    REQUIRE_IMAGES_OR_SKIP(&context);

    void *dummyPtr = reinterpret_cast<void *>(0x17);
    ClSurfaceFormatInfo surfaceFormat;
    image.imageFormat.image_channel_order = CL_BGRA;
    image.imageFormat.image_channel_data_type = CL_UNORM_INT8;

    surfaceFormat.oclImageFormat.image_channel_order = CL_sBGRA;
    surfaceFormat.oclImageFormat.image_channel_data_type = CL_UNORM_INT8;
    descriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    descriptor.image_height = 0;
    descriptor.image_width = 0;
    descriptor.image_row_pitch = image.getHostPtrRowPitch();
    descriptor.image_slice_pitch = image.getHostPtrSlicePitch();
    image.imageDesc = descriptor;
    descriptor.mem_object = &image;

    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, Image::validate(&context, {}, &surfaceFormat, &descriptor, dummyPtr));
};
