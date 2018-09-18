/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "config.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/sharings/gl/gl_texture.h"
#include "gtest/gtest.h"

namespace OCLRT {
namespace glTypes {
static const std::tuple<int, uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/> allImageFormats[] = {
    // input, output, output
    std::make_tuple(GL_RGBA8, CL_UNORM_INT8, CL_RGBA),
    std::make_tuple(GL_RGBA8I, CL_SIGNED_INT8, CL_RGBA),
    std::make_tuple(GL_RGBA16, CL_UNORM_INT16, CL_RGBA),
    std::make_tuple(GL_RGBA16I, CL_SIGNED_INT16, CL_RGBA),
    std::make_tuple(GL_RGBA32I, CL_SIGNED_INT32, CL_RGBA),
    std::make_tuple(GL_RGBA8UI, CL_UNSIGNED_INT8, CL_RGBA),
    std::make_tuple(GL_RGBA16UI, CL_UNSIGNED_INT16, CL_RGBA),
    std::make_tuple(GL_RGBA32UI, CL_UNSIGNED_INT32, CL_RGBA),
    std::make_tuple(GL_RGBA16F, CL_HALF_FLOAT, CL_RGBA),
    std::make_tuple(GL_RGBA32F, CL_FLOAT, CL_RGBA),
    std::make_tuple(GL_RGBA, CL_UNORM_INT8, CL_RGBA),
    std::make_tuple(GL_RGBA8_SNORM, CL_SNORM_INT8, CL_RGBA),
    std::make_tuple(GL_RGBA16_SNORM, CL_SNORM_INT16, CL_RGBA),
    std::make_tuple(GL_BGRA, CL_UNORM_INT8, CL_BGRA),
    std::make_tuple(GL_R8, CL_UNORM_INT8, CL_R),
    std::make_tuple(GL_R8_SNORM, CL_SNORM_INT8, CL_R),
    std::make_tuple(GL_R16, CL_UNORM_INT16, CL_R),
    std::make_tuple(GL_R16_SNORM, CL_SNORM_INT16, CL_R),
    std::make_tuple(GL_R16F, CL_HALF_FLOAT, CL_R),
    std::make_tuple(GL_R32F, CL_FLOAT, CL_R),
    std::make_tuple(GL_R8I, CL_SIGNED_INT8, CL_R),
    std::make_tuple(GL_R16I, CL_SIGNED_INT16, CL_R),
    std::make_tuple(GL_R32I, CL_SIGNED_INT32, CL_R),
    std::make_tuple(GL_R8UI, CL_UNSIGNED_INT8, CL_R),
    std::make_tuple(GL_R16UI, CL_UNSIGNED_INT16, CL_R),
    std::make_tuple(GL_R32UI, CL_UNSIGNED_INT32, CL_R),
    std::make_tuple(GL_DEPTH_COMPONENT32F, CL_FLOAT, CL_DEPTH),
    std::make_tuple(GL_DEPTH_COMPONENT16, CL_UNORM_INT16, CL_DEPTH),
    std::make_tuple(GL_DEPTH24_STENCIL8, CL_UNORM_INT24, CL_DEPTH_STENCIL),
    std::make_tuple(GL_DEPTH32F_STENCIL8, CL_FLOAT, CL_DEPTH_STENCIL),
    std::make_tuple(GL_SRGB8_ALPHA8, CL_UNORM_INT8, CL_sRGBA),
    std::make_tuple(GL_RG8, CL_UNORM_INT8, CL_RG),
    std::make_tuple(GL_RG8_SNORM, CL_SNORM_INT8, CL_RG),
    std::make_tuple(GL_RG16, CL_UNORM_INT16, CL_RG),
    std::make_tuple(GL_RG16_SNORM, CL_SNORM_INT16, CL_RG),
    std::make_tuple(GL_RG16F, CL_HALF_FLOAT, CL_RG),
    std::make_tuple(GL_RG32F, CL_FLOAT, CL_RG),
    std::make_tuple(GL_RG8I, CL_SIGNED_INT8, CL_RG),
    std::make_tuple(GL_RG16I, CL_SIGNED_INT16, CL_RG),
    std::make_tuple(GL_RG32I, CL_SIGNED_INT32, CL_RG),
    std::make_tuple(GL_RG8UI, CL_UNSIGNED_INT8, CL_RG),
    std::make_tuple(GL_RG16UI, CL_UNSIGNED_INT16, CL_RG),
    std::make_tuple(GL_RG32UI, CL_UNSIGNED_INT32, CL_RG),
    std::make_tuple(GL_RGB10, CL_UNORM_INT16, CL_RGBA),
    std::make_tuple(CL_INVALID_VALUE, 0, 0)};

static const std::tuple<unsigned int, uint32_t /*cl_gl_object_type*/, uint32_t /*cl_mem_object_type*/> allObjTypes[] = {
    // input, output, output
    std::make_tuple(GL_TEXTURE_1D, CL_GL_OBJECT_TEXTURE1D, CL_MEM_OBJECT_IMAGE1D),
    std::make_tuple(GL_TEXTURE_1D_ARRAY, CL_GL_OBJECT_TEXTURE1D_ARRAY, CL_MEM_OBJECT_IMAGE1D_ARRAY),
    std::make_tuple(GL_TEXTURE_2D, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_RECTANGLE, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_CUBE_MAP_POSITIVE_X, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_2D_MULTISAMPLE, CL_GL_OBJECT_TEXTURE2D, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(GL_TEXTURE_2D_ARRAY, CL_GL_OBJECT_TEXTURE2D_ARRAY, CL_MEM_OBJECT_IMAGE2D_ARRAY),
    std::make_tuple(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, CL_GL_OBJECT_TEXTURE2D_ARRAY, CL_MEM_OBJECT_IMAGE2D_ARRAY),
    std::make_tuple(GL_TEXTURE_3D, CL_GL_OBJECT_TEXTURE3D, CL_MEM_OBJECT_IMAGE3D),
    std::make_tuple(GL_TEXTURE_BUFFER, CL_GL_OBJECT_TEXTURE_BUFFER, CL_MEM_OBJECT_IMAGE1D_BUFFER),
    std::make_tuple(GL_RENDERBUFFER_EXT, CL_GL_OBJECT_RENDERBUFFER, CL_MEM_OBJECT_IMAGE2D),
    std::make_tuple(CL_INVALID_VALUE, 0, 0)};
} // namespace glTypes
struct GlClImageFormatTests
    : public ::testing::WithParamInterface<std::tuple<int, uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/>>,
      public ::testing::Test {};

INSTANTIATE_TEST_CASE_P(GlClImageFormatTests, GlClImageFormatTests, testing::ValuesIn(glTypes::allImageFormats));

TEST_P(GlClImageFormatTests, validFormat) {
    cl_image_format imgFormat = {};
    auto glFormat = std::get<0>(GetParam());
    auto expectedClChannelType = static_cast<cl_channel_type>(std::get<1>(GetParam()));
    auto expectedClChannelOrder = static_cast<cl_channel_order>(std::get<2>(GetParam()));

    GlTexture::setClImageFormat(glFormat, imgFormat);

    EXPECT_EQ(imgFormat.image_channel_data_type, expectedClChannelType);
    EXPECT_EQ(imgFormat.image_channel_order, expectedClChannelOrder);
}

struct GlClObjTypesTests
    : public ::testing::WithParamInterface<std::tuple<unsigned int, uint32_t /*cl_gl_object_type*/, uint32_t /*cl_mem_object_type*/>>,
      public ::testing::Test {};

INSTANTIATE_TEST_CASE_P(GlClObjTypesTests, GlClObjTypesTests, testing::ValuesIn(glTypes::allObjTypes));

TEST_P(GlClObjTypesTests, typeConversion) {
    auto glType = static_cast<cl_GLenum>(std::get<0>(GetParam()));
    auto expectedClGlObjType = static_cast<cl_gl_object_type>(std::get<1>(GetParam()));
    auto expectedClMemObjType = static_cast<cl_mem_object_type>(std::get<2>(GetParam()));

    auto clGlObjType = GlTexture::getClGlObjectType(glType);
    auto clMemObjType = GlTexture::getClMemObjectType(glType);

    EXPECT_EQ(expectedClGlObjType, clGlObjType);
    EXPECT_EQ(clMemObjType, expectedClMemObjType);
}
} // namespace OCLRT
