/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "d3d_sharing_functions.h"

#include <mutex>

enum GMM_RESOURCE_FORMAT_ENUM;
namespace NEO {
enum class ImagePlane;
class Context;
class Gmm;
struct ClSurfaceFormatInfo;
struct ImageInfo;

template <typename D3D>
class D3DSharing : public SharingHandler {
    typedef typename D3D::D3DQuery D3DQuery;
    typedef typename D3D::D3DResource D3DResource;

  public:
    D3DSharing(Context *context, D3DResource *resource, D3DResource *resourceStaging, unsigned int subresource, bool sharedResource);

    ~D3DSharing() override;

    void synchronizeObject(UpdateData &updateData) override;
    void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override;

    D3DResource **getResourceHandler() { return &resource; }
    void *getResourceStaging() { return resourceStaging; }
    unsigned int &getSubresource() { return subresource; }
    typename D3DQuery *getQuery() { return d3dQuery; }
    bool isSharedResource() { return sharedResource; }
    static const ClSurfaceFormatInfo *findSurfaceFormatInfo(GMM_RESOURCE_FORMAT_ENUM gmmFormat, cl_mem_flags flags, bool supportsOcl20Features, bool packedSupported);
    static bool isFormatWithPlane1(DXGI_FORMAT format);

  protected:
    static void updateImgInfoAndDesc(Gmm *gmm, ImageInfo &imgInfo, ImagePlane imagePlane, cl_uint arrayIndex);

    Context *context;
    D3DSharingFunctions<D3D> *sharingFunctions = nullptr;
    D3DResource *resource = nullptr;
    D3DResource *resourceStaging = nullptr;
    D3DQuery *d3dQuery = nullptr;
    bool sharedResource = false;
    unsigned int subresource = 0;
    std::mutex mtx;
};
} // namespace NEO
