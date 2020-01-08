/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/sharings/d3d/d3d_sharing.h"

#include "d3d_sharing_functions.h"

enum GMM_RESOURCE_FORMAT_ENUM;
namespace NEO {
enum class OCLPlane;
class Context;
class Gmm;
struct SurfaceFormatInfo;
struct ImageInfo;

template <typename D3D>
class D3DSharing : public SharingHandler {
    typedef typename D3D::D3DQuery D3DQuery;
    typedef typename D3D::D3DResource D3DResource;

  public:
    D3DSharing(Context *context, D3DResource *resource, D3DResource *resourceStaging, unsigned int subresource, bool sharedResource);

    ~D3DSharing() override;

    void synchronizeObject(UpdateData &updateData) override;
    void releaseResource(MemObj *memObject) override;

    D3DResource **getResourceHandler() { return &resource; }
    void *getResourceStaging() { return resourceStaging; }
    unsigned int &getSubresource() { return subresource; }
    typename D3DQuery *getQuery() { return d3dQuery; }
    bool isSharedResource() { return sharedResource; }
    static const SurfaceFormatInfo *findSurfaceFormatInfo(GMM_RESOURCE_FORMAT_ENUM gmmFormat, cl_mem_flags flags);
    static bool isFormatWithPlane1(DXGI_FORMAT format);

  protected:
    static void updateImgInfoAndDesc(Gmm *gmm, ImageInfo &imgInfo, OCLPlane oclPlane, cl_uint arrayIndex);

    Context *context;
    D3DSharingFunctions<D3D> *sharingFunctions = nullptr;
    D3DResource *resource = nullptr;
    D3DResource *resourceStaging = nullptr;
    D3DQuery *d3dQuery = nullptr;
    bool sharedResource = false;
    unsigned int subresource = 0;
};
} // namespace NEO
