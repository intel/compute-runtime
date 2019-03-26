/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/sharings/d3d/d3d_sharing.h"

namespace NEO {
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
} // namespace NEO
