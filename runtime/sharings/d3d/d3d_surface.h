/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include <map>

struct ErrorCodeHelper;
namespace OCLRT {
enum class OCLPlane;
class Image;
class Context;

class D3DSurface : public D3DSharing<D3DTypesHelper::D3D9> {
    typedef typename D3DTypesHelper::D3D9::D3DTexture2dDesc D3D9SurfaceDesc;
    typedef typename D3DTypesHelper::D3D9::D3DTexture2d D3D9Surface;
    typedef typename D3DTypesHelper::D3D9::D3DResource D3DResource;
    typedef typename D3DTypesHelper::D3D9::D3DDevice D3DDevice;

  public:
    static Image *create(Context *context, cl_dx9_surface_info_khr *surfaceInfo, cl_mem_flags flags,
                         cl_dx9_media_adapter_type_khr adapterType, cl_uint plane, cl_int *retCode);
    static const std::map<const D3DFORMAT, const cl_image_format> D3DFMTCLImageFormat;
    static cl_int findImgFormat(D3DFORMAT d3dFormat, cl_image_format &imgFormat, cl_uint plane, OCLPlane &oclPlane);
    void synchronizeObject(UpdateData &updateData) override;
    void releaseResource(MemObj *memObject) override;
    int validateUpdateData(UpdateData &updateData) override;

    cl_dx9_surface_info_khr &getSurfaceInfo() { return surfaceInfo; }
    cl_dx9_media_adapter_type_khr &getAdapterType() { return adapterType; }
    cl_uint &getPlane() { return plane; }

    ~D3DSurface() override = default;
    const bool lockable = false;

  protected:
    D3DSurface(Context *context, cl_dx9_surface_info_khr *surfaceInfo, D3D9Surface *surfaceStaging, cl_uint plane,
               OCLPlane oclPlane, cl_dx9_media_adapter_type_khr adapterType, bool sharedResource, bool lockable);
    cl_dx9_media_adapter_type_khr adapterType = 0u;
    cl_dx9_surface_info_khr surfaceInfo = {};
    cl_uint plane = 0;
    OCLPlane oclPlane;

    D3D9Surface *d3d9Surface = nullptr;
    D3D9Surface *d3d9SurfaceStaging = nullptr;
    D3DDevice *resourceDevice = nullptr;
};
} // namespace OCLRT
