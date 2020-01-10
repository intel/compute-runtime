/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "surface_formats.h"

#include "core/gmm_helper/gmm_lib.h"
#include "core/helpers/array_count.h"
#include "runtime/api/cl_types.h"
#include "runtime/mem_obj/image.h"

#include "validators.h"

namespace NEO {

// clang-format off
#define COMMONFORMATS \
    {{CL_RGBA,            CL_UNORM_INT8},     {GMM_FORMAT_R8G8B8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM          , 0, 4, 1, 4}}, \
    {{CL_RGBA,            CL_UNORM_INT16},    {GMM_FORMAT_R16G16B16A16_UNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM      , 0, 4, 2, 8}}, \
    {{CL_RGBA,            CL_SIGNED_INT8},    {GMM_FORMAT_R8G8B8A8_SINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SINT           , 0, 4, 1, 4}}, \
    {{CL_RGBA,            CL_SIGNED_INT16},   {GMM_FORMAT_R16G16B16A16_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SINT       , 0, 4, 2, 8}}, \
    {{CL_RGBA,            CL_SIGNED_INT32},   {GMM_FORMAT_R32G32B32A32_SINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_SINT       , 0, 4, 4, 16}}, \
    {{CL_RGBA,            CL_UNSIGNED_INT8},  {GMM_FORMAT_R8G8B8A8_UINT_TYPE,       GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UINT           , 0, 4, 1, 4}}, \
    {{CL_RGBA,            CL_UNSIGNED_INT16}, {GMM_FORMAT_R16G16B16A16_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UINT       , 0, 4, 2, 8}}, \
    {{CL_RGBA,            CL_UNSIGNED_INT32}, {GMM_FORMAT_R32G32B32A32_UINT_TYPE,   GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_UINT       , 0, 4, 4, 16}}, \
    {{CL_RGBA,            CL_HALF_FLOAT},     {GMM_FORMAT_R16G16B16A16_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_FLOAT      , 0, 4, 2, 8}}, \
    {{CL_RGBA,            CL_FLOAT},          {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE,  GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_FLOAT      , 0, 4, 4, 16}}, \
    {{CL_BGRA,            CL_UNORM_INT8},     {GMM_FORMAT_B8G8R8A8_UNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_B8G8R8A8_UNORM          , 0, 4, 1, 4}}, \
    {{CL_R,               CL_FLOAT},          {GMM_FORMAT_R32_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4}}, \
    {{CL_R,               CL_UNORM_INT8},     {GMM_FORMAT_R8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1}}, \
    {{CL_R,               CL_UNORM_INT16},    {GMM_FORMAT_R16_UNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2}}, \
    {{CL_R,               CL_SIGNED_INT8},    {GMM_FORMAT_R8_SINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_SINT                 , 0, 1, 1, 1}}, \
    {{CL_R,               CL_SIGNED_INT16},   {GMM_FORMAT_R16_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_SINT                , 0, 1, 2, 2}}, \
    {{CL_R,               CL_SIGNED_INT32},   {GMM_FORMAT_R32_SINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_SINT                , 0, 1, 4, 4}}, \
    {{CL_R,               CL_UNSIGNED_INT8},  {GMM_FORMAT_R8_UINT_TYPE,             GFX3DSTATE_SURFACEFORMAT_R8_UINT                 , 0, 1, 1, 1}}, \
    {{CL_R,               CL_UNSIGNED_INT16}, {GMM_FORMAT_R16_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R16_UINT                , 0, 1, 2, 2}}, \
    {{CL_R,               CL_UNSIGNED_INT32}, {GMM_FORMAT_R32_UINT_TYPE,            GFX3DSTATE_SURFACEFORMAT_R32_UINT                , 0, 1, 4, 4}}, \
    {{CL_R,               CL_HALF_FLOAT},     {GMM_FORMAT_R16_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2}}, \
    {{CL_A,               CL_UNORM_INT8},     {GMM_FORMAT_A8_UNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_A8_UNORM                , 0, 1, 1, 1}}, \
    {{CL_RG,              CL_UNORM_INT8},     {GMM_FORMAT_R8G8_UNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_UNORM              , 0, 2, 1, 2}}, \
    {{CL_RG,              CL_UNORM_INT16},    {GMM_FORMAT_R16G16_UNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_UNORM            , 0, 2, 2, 4}}, \
    {{CL_RG,              CL_SIGNED_INT8},    {GMM_FORMAT_R8G8_SINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_SINT               , 0, 2, 1, 2}}, \
    {{CL_RG,              CL_SIGNED_INT16},   {GMM_FORMAT_R16G16_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_SINT             , 0, 2, 2, 4}}, \
    {{CL_RG,              CL_SIGNED_INT32},   {GMM_FORMAT_R32G32_SINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_SINT             , 0, 2, 4, 8}}, \
    {{CL_RG,              CL_UNSIGNED_INT8},  {GMM_FORMAT_R8G8_UINT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8G8_UINT               , 0, 2, 1, 2}}, \
    {{CL_RG,              CL_UNSIGNED_INT16}, {GMM_FORMAT_R16G16_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R16G16_UINT             , 0, 2, 2, 4}}, \
    {{CL_RG,              CL_UNSIGNED_INT32}, {GMM_FORMAT_R32G32_UINT_TYPE,         GFX3DSTATE_SURFACEFORMAT_R32G32_UINT             , 0, 2, 4, 8}}, \
    {{CL_RG,              CL_HALF_FLOAT},     {GMM_FORMAT_R16G16_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT            , 0, 2, 2, 4}}, \
    {{CL_RG,              CL_FLOAT},          {GMM_FORMAT_R32G32_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R32G32_FLOAT            , 0, 2, 4, 8}}, \
    {{CL_LUMINANCE,       CL_UNORM_INT8},     {GMM_FORMAT_GENERIC_8BIT,             GFX3DSTATE_SURFACEFORMAT_R8_UNORM                , 0, 1, 1, 1}}, \
    {{CL_LUMINANCE,       CL_UNORM_INT16},    {GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2}}, \
    {{CL_LUMINANCE,       CL_HALF_FLOAT},     {GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_R16_FLOAT               , 0, 1, 2, 2}}, \
    {{CL_LUMINANCE,       CL_FLOAT},          {GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4}}, \
    {{CL_R,               CL_SNORM_INT8},     {GMM_FORMAT_R8_SNORM_TYPE,            GFX3DSTATE_SURFACEFORMAT_R8_SNORM                , 0, 1, 1, 1}}, \
    {{CL_R,               CL_SNORM_INT16},    {GMM_FORMAT_R16_SNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_SNORM               , 0, 1, 2, 2}}, \
    {{CL_RG,              CL_SNORM_INT8},     {GMM_FORMAT_R8G8_SNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM              , 0, 2, 1, 2}}, \
    {{CL_RG,              CL_SNORM_INT16},    {GMM_FORMAT_R16G16_SNORM_TYPE,        GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM            , 0, 2, 2, 4}}, \
    {{CL_RGBA,            CL_SNORM_INT8},     {GMM_FORMAT_R8G8B8A8_SNORM_TYPE,      GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM          , 0, 4, 1, 4}}, \
    {{CL_RGBA,            CL_SNORM_INT16},    {GMM_FORMAT_R16G16B16A16_SNORM_TYPE,  GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SNORM      , 0, 4, 2, 8}}

#define READONLYFORMATS \
    {{CL_INTENSITY,       CL_UNORM_INT8},     {GMM_FORMAT_GENERIC_8BIT,             GFX3DSTATE_SURFACEFORMAT_I8_UNORM                , 0, 1, 1, 1}}, \
    {{CL_INTENSITY,       CL_UNORM_INT16},    {GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_I16_UNORM               , 0, 1, 2, 2}}, \
    {{CL_INTENSITY,       CL_HALF_FLOAT},     {GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_I16_FLOAT               , 0, 1, 2, 2}}, \
    {{CL_INTENSITY,       CL_FLOAT},          {GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_I32_FLOAT               , 0, 1, 4, 4}}, \
    {{CL_A,               CL_UNORM_INT16},    {GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_A16_UNORM               , 0, 1, 2, 2}}, \
    {{CL_A,               CL_HALF_FLOAT},     {GMM_FORMAT_GENERIC_16BIT,            GFX3DSTATE_SURFACEFORMAT_A16_FLOAT               , 0, 1, 2, 2}}, \
    {{CL_A,               CL_FLOAT},          {GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_A32_FLOAT               , 0, 1, 4, 4}}

#define SRGBFORMATS \
    {{CL_sRGBA,           CL_UNORM_INT8},     {GMM_FORMAT_R8G8B8A8_UNORM_SRGB_TYPE, GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM_SRGB     , 0, 4, 1, 4}}, \
    {{CL_sBGRA,           CL_UNORM_INT8},     {GMM_FORMAT_B8G8R8A8_UNORM_SRGB_TYPE, GFX3DSTATE_SURFACEFORMAT_B8G8R8A8_UNORM_SRGB     , 0, 4, 1, 4}}

#define DEPTHFORMATS \
    {{ CL_DEPTH,          CL_FLOAT},         {GMM_FORMAT_R32_FLOAT_TYPE,           GFX3DSTATE_SURFACEFORMAT_R32_FLOAT               , 0, 1, 4, 4}}, \
    {{ CL_DEPTH,          CL_UNORM_INT16},   {GMM_FORMAT_R16_UNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R16_UNORM               , 0, 1, 2, 2}}

#define DEPTHSTENCILFORMATS \
    {{ CL_DEPTH_STENCIL,  CL_UNORM_INT24},   {GMM_FORMAT_GENERIC_32BIT,            GFX3DSTATE_SURFACEFORMAT_R24_UNORM_X8_TYPELESS   , 0, 1, 4, 4}}, \
    {{ CL_DEPTH_STENCIL,  CL_FLOAT},         {GMM_FORMAT_R32G32_FLOAT_TYPE,        GFX3DSTATE_SURFACEFORMAT_R32_FLOAT_X8X24_TYPELESS, 0, 2, 4, 8}}

//Initialize this with the required formats first.
//Append the optional one later
const ClSurfaceFormatInfo SurfaceFormats::readOnlySurfaceFormats12[] = { COMMONFORMATS, READONLYFORMATS };
const ClSurfaceFormatInfo SurfaceFormats::readOnlySurfaceFormats20[] = { COMMONFORMATS, READONLYFORMATS, SRGBFORMATS };

const ClSurfaceFormatInfo SurfaceFormats::writeOnlySurfaceFormats[] = { COMMONFORMATS };

const ClSurfaceFormatInfo SurfaceFormats::readWriteSurfaceFormats[] = { COMMONFORMATS };

const ClSurfaceFormatInfo SurfaceFormats::packedYuvSurfaceFormats[] = {
    {{CL_YUYV_INTEL,      CL_UNORM_INT8},     {GMM_FORMAT_YUY2,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL            , 0, 2, 1, 2}},
    {{CL_UYVY_INTEL,      CL_UNORM_INT8},     {GMM_FORMAT_UYVY,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY             , 0, 2, 1, 2}},
    {{CL_YVYU_INTEL,      CL_UNORM_INT8},     {GMM_FORMAT_YVYU,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV            , 0, 2, 1, 2}},
    {{CL_VYUY_INTEL,      CL_UNORM_INT8},     {GMM_FORMAT_VYUY,                     GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY           , 0, 2, 1, 2}}
};

const ClSurfaceFormatInfo SurfaceFormats::planarYuvSurfaceFormats[] = {
    {{CL_NV12_INTEL,      CL_UNORM_INT8},     {GMM_FORMAT_NV12,                     GFX3DSTATE_SURFACEFORMAT_NV12                    , 0, 1, 1, 1}}
};

const ClSurfaceFormatInfo SurfaceFormats::readOnlyDepthSurfaceFormats[] = { DEPTHFORMATS, DEPTHSTENCILFORMATS };

const ClSurfaceFormatInfo SurfaceFormats::readWriteDepthSurfaceFormats[] = { DEPTHFORMATS };

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::readOnly12() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(readOnlySurfaceFormats12);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::readOnly20() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(readOnlySurfaceFormats20);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::writeOnly() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(writeOnlySurfaceFormats);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::readWrite() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(readWriteSurfaceFormats);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::packedYuv() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(packedYuvSurfaceFormats);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::planarYuv() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(planarYuvSurfaceFormats);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::readOnlyDepth() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(readOnlyDepthSurfaceFormats);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::readWriteDepth() noexcept {
    return ArrayRef<const ClSurfaceFormatInfo>(readWriteDepthSurfaceFormats);
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::surfaceFormats(cl_mem_flags flags, unsigned int clVersionSupport) noexcept {
    if (flags & CL_MEM_READ_ONLY) {
        if(clVersionSupport >= 20 ) {
            return readOnly20();
        }
        else {
            return readOnly12();
        }
    }
    else if (flags & CL_MEM_WRITE_ONLY) {
        return writeOnly();
    }
    else {
        return readWrite();
    }
}

ArrayRef<const ClSurfaceFormatInfo> SurfaceFormats::surfaceFormats(cl_mem_flags flags, const cl_image_format *imageFormat, unsigned int clVersionSupport) noexcept {
    if (NEO::IsNV12Image(imageFormat)) {
        return planarYuv();
    }
    else if (IsPackedYuvImage(imageFormat)) {
        return packedYuv();
    }
    else if (Image::isDepthFormat(*imageFormat)) {
        if (flags & CL_MEM_READ_ONLY) {
            return readOnlyDepth();
        }
        else {
            return readWriteDepth();
        }
    }
    else if (flags & CL_MEM_READ_ONLY) {
        if(clVersionSupport >= 20 ) {
            return readOnly20();
        }
        else {
            return readOnly12();
        }
    }
    else if (flags & CL_MEM_WRITE_ONLY) {
        return writeOnly();
    }
    else {
        return readWrite();
    }
}

// clang-format on
} // namespace NEO
