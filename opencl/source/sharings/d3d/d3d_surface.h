/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/sharings/d3d/d3d_sharing.h"

#include <map>

struct ErrorCodeHelper;
namespace NEO {
enum class ImagePlane;
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
    static const std::map<const D3DFORMAT, const cl_image_format> D3DtoClFormatConversions;
    static const std::vector<D3DFORMAT> D3DPlane1Formats;
    static const std::vector<D3DFORMAT> D3DPlane2Formats;
    static cl_int findImgFormat(D3DFORMAT d3dFormat, cl_image_format &imgFormat, cl_uint plane, ImagePlane &imagePlane);
    void synchronizeObject(UpdateData &updateData) override;
    void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override;
    int validateUpdateData(UpdateData &updateData) override;

    cl_dx9_surface_info_khr &getSurfaceInfo() { return surfaceInfo; }
    cl_dx9_media_adapter_type_khr &getAdapterType() { return adapterType; }
    cl_uint &getPlane() { return plane; }

    ~D3DSurface() override = default;
    const bool lockable = false;

  protected:
    D3DSurface(Context *context, cl_dx9_surface_info_khr *surfaceInfo, D3D9Surface *surfaceStaging, cl_uint plane,
               ImagePlane imagePlane, cl_dx9_media_adapter_type_khr adapterType, bool sharedResource, bool lockable);
    cl_dx9_media_adapter_type_khr adapterType = 0u;
    cl_dx9_surface_info_khr surfaceInfo = {};
    cl_uint plane = 0;
    ImagePlane imagePlane;

    D3D9Surface *d3d9Surface = nullptr;
    D3D9Surface *d3d9SurfaceStaging = nullptr;
    D3DDevice *resourceDevice = nullptr;
};
} // namespace NEO
