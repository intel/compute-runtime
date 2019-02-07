/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/d3d/d3d_sharing.h"
#include "runtime/context/context.h"
#include "runtime/mem_obj/image.h"
#include "runtime/gmm_helper/gmm.h"

using namespace OCLRT;

template class D3DSharing<D3DTypesHelper::D3D9>;
template class D3DSharing<D3DTypesHelper::D3D10>;
template class D3DSharing<D3DTypesHelper::D3D11>;

template <typename D3D>
D3DSharing<D3D>::D3DSharing(Context *context, D3DResource *resource, D3DResource *resourceStaging, unsigned int subresource, bool sharedResource)
    : sharedResource(sharedResource), subresource(subresource), resource(resource), resourceStaging(resourceStaging), context(context) {
    sharingFunctions = context->getSharing<D3DSharingFunctions<D3D>>();
    if (sharingFunctions) {
        sharingFunctions->addRef(resource);
        sharingFunctions->createQuery(&this->d3dQuery);
        sharingFunctions->track(resource, subresource);
    }
};

template <typename D3D>
D3DSharing<D3D>::~D3DSharing() {
    if (sharingFunctions) {
        sharingFunctions->untrack(resource, subresource);
        if (!sharedResource) {
            sharingFunctions->release(resourceStaging);
        }
        sharingFunctions->release(resource);
        sharingFunctions->release(d3dQuery);
    }
};

template <typename D3D>
void D3DSharing<D3D>::synchronizeObject(UpdateData &updateData) {
    sharingFunctions->getDeviceContext(d3dQuery);
    if (!sharedResource) {
        sharingFunctions->copySubresourceRegion(resourceStaging, 0, resource, subresource);
        sharingFunctions->flushAndWait(d3dQuery);
    } else if (!context->getInteropUserSyncEnabled()) {
        sharingFunctions->flushAndWait(d3dQuery);
    }
    sharingFunctions->releaseDeviceContext(d3dQuery);

    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
}

template <typename D3D>
void D3DSharing<D3D>::releaseResource(MemObj *memObject) {
    if (!sharedResource) {
        sharingFunctions->getDeviceContext(d3dQuery);
        sharingFunctions->copySubresourceRegion(resource, subresource, resourceStaging, 0);
        if (!context->getInteropUserSyncEnabled()) {
            sharingFunctions->flushAndWait(d3dQuery);
        }
        sharingFunctions->releaseDeviceContext(d3dQuery);
    }
}

template <typename D3D>
void D3DSharing<D3D>::updateImgInfo(Gmm *gmm, ImageInfo &imgInfo, cl_image_desc &imgDesc, OCLPlane oclPlane, cl_uint arrayIndex) {

    gmm->updateImgInfo(imgInfo, imgDesc, arrayIndex);

    if (oclPlane == OCLPlane::PLANE_U || oclPlane == OCLPlane::PLANE_V || oclPlane == OCLPlane::PLANE_UV) {
        imgDesc.image_width /= 2;
        imgDesc.image_height /= 2;
        if (oclPlane != OCLPlane::PLANE_UV) {
            imgDesc.image_row_pitch /= 2;
        }
    }
}

template <typename D3D>
const SurfaceFormatInfo *D3DSharing<D3D>::findSurfaceFormatInfo(GMM_RESOURCE_FORMAT_ENUM gmmFormat, cl_mem_flags flags) {
    ArrayRef<const SurfaceFormatInfo> formats = SurfaceFormats::surfaceFormats(flags);
    for (auto &format : formats) {
        if (gmmFormat == format.GMMSurfaceFormat) {
            return &format;
        }
    }
    return nullptr;
}
