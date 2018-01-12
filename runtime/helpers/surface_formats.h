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

#pragma once
#include "config.h"
#include "CL/cl.h"

#ifndef SUPPORT_YUV
#define SUPPORT_YUV 1
#include "CL/cl_ext.h"
#endif

#include "runtime/gmm_helper/gmm_lib.h"

namespace OCLRT {
enum GFX3DSTATE_SURFACEFORMAT : unsigned short;

struct SurfaceFormatInfo {
    cl_image_format OCLImageFormat;
    GMM_RESOURCE_FORMAT GMMSurfaceFormat;
    GFX3DSTATE_SURFACEFORMAT GenxSurfaceFormat;
    cl_uint GMMTileWalk;
    cl_uint NumChannels;
    cl_uint PerChannelSizeInBytes;
    size_t ImageElementSizeInBytes;
};

struct ImageInfo {
    const cl_image_desc *imgDesc;
    const SurfaceFormatInfo *surfaceFormat;
    size_t size;
    size_t rowPitch;
    size_t slicePitch;
    uint32_t qPitch;
    uint32_t offset;
    uint32_t xOffset;
    uint32_t yOffset;
    uint32_t yOffsetForUVPlane;
    GMM_YUV_PLANE_ENUM plane;
    int mipLevel;
    bool preferRenderCompression;
};

struct McsSurfaceInfo {
    uint32_t pitch;
    uint32_t qPitch;
    uint32_t multisampleCount;
};

extern const size_t numReadOnlySurfaceFormats;
extern const size_t numWriteOnlySurfaceFormats;
extern const size_t numReadWriteSurfaceFormats;

extern const SurfaceFormatInfo readOnlySurfaceFormats[];
extern const SurfaceFormatInfo writeOnlySurfaceFormats[];
extern const SurfaceFormatInfo readWriteSurfaceFormats[];
extern const SurfaceFormatInfo noAccessSurfaceFormats[];

#ifdef SUPPORT_YUV
extern const size_t numPackedYuvSurfaceFormats;
extern const size_t numPlanarYuvSurfaceFormats;
extern const SurfaceFormatInfo packedYuvSurfaceFormats[];
extern const SurfaceFormatInfo planarYuvSurfaceFormats[];
#endif

extern const size_t numReadOnlyDepthSurfaceFormats;
extern const size_t numReadWriteDepthSurfaceFormats;

extern const SurfaceFormatInfo readOnlyDepthSurfaceFormats[];
extern const SurfaceFormatInfo readWriteDepthSurfaceFormats[];

extern const size_t numSnormSurfaceFormats;
extern const SurfaceFormatInfo snormSurfaceFormats[];
} // namespace OCLRT
