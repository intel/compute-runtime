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
#include "runtime/sharings/d3d/d3d_sharing.h"

namespace OCLRT {
class Context;
class Image;

template <typename D3D>
class D3DTexture : public D3DSharing<D3D> {
    typedef typename D3D::D3DTexture2dDesc D3DTexture2dDesc;
    typedef typename D3D::D3DTexture3dDesc D3DTexture3dDesc;
    typedef typename D3D::D3DTexture2d D3DTexture2d;
    typedef typename D3D::D3DTexture3d D3DTexture3d;
    typedef typename D3D::D3DResource D3DResource;

  public:
    ~D3DTexture() override = default;

    static Image *create2d(Context *context, D3DTexture2d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode);

    static Image *create3d(Context *context, D3DTexture3d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode);

    static const SurfaceFormatInfo *findYuvSurfaceFormatInfo(DXGI_FORMAT dxgiFormat, OCLPlane oclPlane, cl_mem_flags flags);

  protected:
    D3DTexture(Context *context, D3DResource *d3dTexture, cl_uint subresource, D3DResource *textureStaging, bool sharedResource)
        : D3DSharing(context, d3dTexture, textureStaging, subresource, sharedResource){};
};
} // namespace OCLRT
