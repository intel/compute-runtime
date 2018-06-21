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
#include "d3d_sharing_functions.h"

enum GMM_RESOURCE_FORMAT_ENUM;
namespace OCLRT {
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

    void synchronizeObject(UpdateData *updateData) override;
    void releaseResource(MemObj *memObject) override;

    D3DResource **getResourceHandler() { return &resource; }
    void *getResourceStaging() { return resourceStaging; }
    unsigned int &getSubresource() { return subresource; }
    typename D3DQuery *getQuery() { return d3dQuery; }
    bool isSharedResource() { return sharedResource; }
    static const SurfaceFormatInfo *findSurfaceFormatInfo(GMM_RESOURCE_FORMAT_ENUM gmmFormat, cl_mem_flags flags);

  protected:
    static void updateImgInfo(Gmm *gmm, ImageInfo &imgInfo, cl_image_desc &imgDesc, OCLPlane oclPlane, cl_uint arrayIndex);

    Context *context;
    D3DSharingFunctions<D3D> *sharingFunctions = nullptr;
    D3DResource *resource = nullptr;
    D3DResource *resourceStaging = nullptr;
    D3DQuery *d3dQuery = nullptr;
    bool sharedResource = false;
    unsigned int subresource = 0;
};
} // namespace OCLRT
