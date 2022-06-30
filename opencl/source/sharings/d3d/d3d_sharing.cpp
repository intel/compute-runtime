/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/d3d/d3d_sharing.h"

#include "shared/source/gmm_helper/gmm.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"

using namespace NEO;

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
    std::unique_lock<std::mutex> lock(this->mtx);
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
void D3DSharing<D3D>::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
    if (!sharedResource) {
        std::unique_lock<std::mutex> lock(this->mtx);
        sharingFunctions->getDeviceContext(d3dQuery);
        sharingFunctions->copySubresourceRegion(resource, subresource, resourceStaging, 0);
        if (!context->getInteropUserSyncEnabled()) {
            sharingFunctions->flushAndWait(d3dQuery);
        }
        sharingFunctions->releaseDeviceContext(d3dQuery);
    }
}

template <typename D3D>
void D3DSharing<D3D>::updateImgInfoAndDesc(Gmm *gmm, ImageInfo &imgInfo, ImagePlane imagePlane, cl_uint arrayIndex) {

    gmm->updateImgInfoAndDesc(imgInfo, arrayIndex);

    if (imagePlane == ImagePlane::PLANE_U || imagePlane == ImagePlane::PLANE_V || imagePlane == ImagePlane::PLANE_UV) {
        imgInfo.imgDesc.imageWidth /= 2;
        imgInfo.imgDesc.imageHeight /= 2;
        if (imagePlane != ImagePlane::PLANE_UV) {
            imgInfo.imgDesc.imageRowPitch /= 2;
        }
    }
}

template <typename D3D>
const ClSurfaceFormatInfo *D3DSharing<D3D>::findSurfaceFormatInfo(GMM_RESOURCE_FORMAT_ENUM gmmFormat, cl_mem_flags flags, bool supportsOcl20Features, bool packedSupported) {
    ArrayRef<const ClSurfaceFormatInfo> formats = SurfaceFormats::surfaceFormats(flags, supportsOcl20Features);
    for (auto &format : formats) {
        if (gmmFormat == format.surfaceFormat.GMMSurfaceFormat) {
            return &format;
        }
    }
    if (packedSupported) {
        formats = SurfaceFormats::packed();
        for (auto &format : formats) {
            if (gmmFormat == format.surfaceFormat.GMMSurfaceFormat) {
                return &format;
            }
        }
    }

    return nullptr;
}

template <typename D3D>
bool D3DSharing<D3D>::isFormatWithPlane1(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_P208:
        return true;
    }
    return false;
}
