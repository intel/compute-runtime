/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/d3d/d3d_texture.h"

#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/get_info.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/memory_manager.h"

using namespace OCLRT;

template class D3DTexture<D3DTypesHelper::D3D10>;
template class D3DTexture<D3DTypesHelper::D3D11>;

template <typename D3D>
Image *D3DTexture<D3D>::create2d(Context *context, D3DTexture2d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    OCLPlane oclPlane = OCLPlane::NO_PLANE;
    void *sharedHandle = nullptr;
    cl_uint arrayIndex = 0u;
    cl_image_desc imgDesc = {};
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};
    ImageInfo imgInfo = {};
    imgInfo.imgDesc = &imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    D3DTexture2dDesc textureDesc = {};
    sharingFcns->getTexture2dDesc(&textureDesc, d3dTexture);

    if ((textureDesc.Format == DXGI_FORMAT_NV12) || (textureDesc.Format == DXGI_FORMAT_P010) || (textureDesc.Format == DXGI_FORMAT_P016)) {
        if ((subresource % 2) == 0) {
            oclPlane = OCLPlane::PLANE_Y;
        } else {
            oclPlane = OCLPlane::PLANE_UV;
        }
        imgInfo.plane = GmmHelper::convertPlane(oclPlane);
        arrayIndex = subresource / 2u;
    } else if (subresource >= textureDesc.MipLevels * textureDesc.ArraySize) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    bool sharedResource = false;
    D3DTexture2d *textureStaging = nullptr;
    if ((textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED ||
         textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_KEYEDMUTEX) &&
        subresource % textureDesc.MipLevels == 0) {
        textureStaging = d3dTexture;
        sharedResource = true;
    } else {
        sharingFcns->createTexture2d(&textureStaging, &textureDesc, subresource);
    }

    GraphicsAllocation *alloc = nullptr;
    if (textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE) {
        sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle);
        alloc = context->getMemoryManager()->createGraphicsAllocationFromNTHandle(sharedHandle);
    } else {
        sharingFcns->getSharedHandle(textureStaging, &sharedHandle);
        alloc = context->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)((UINT_PTR)sharedHandle), false);
    }
    DEBUG_BREAK_IF(!alloc);

    updateImgInfo(alloc->getDefaultGmm(), imgInfo, imgDesc, oclPlane, arrayIndex);

    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);

    if ((textureDesc.Format == DXGI_FORMAT_NV12) || (textureDesc.Format == DXGI_FORMAT_P010) || (textureDesc.Format == DXGI_FORMAT_P016)) {
        imgInfo.surfaceFormat = findYuvSurfaceFormatInfo(textureDesc.Format, oclPlane, flags);
    } else {
        imgInfo.surfaceFormat = findSurfaceFormatInfo(alloc->getDefaultGmm()->gmmResourceInfo->getResourceFormat(), flags);
    }

    if (alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        alloc->getDefaultGmm()->isRenderCompressed = context->getMemoryManager()->mapAuxGpuVA(alloc);
    }

    return Image::createSharedImage(context, d3dTextureObj, mcsSurfaceInfo, alloc, nullptr, flags, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
}

template <typename D3D>
Image *D3DTexture<D3D>::create3d(Context *context, D3DTexture3d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    void *sharedHandle = nullptr;
    cl_image_desc imgDesc = {};
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};
    ImageInfo imgInfo = {};
    imgInfo.imgDesc = &imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;

    D3DTexture3dDesc textureDesc = {};
    sharingFcns->getTexture3dDesc(&textureDesc, d3dTexture);

    if (subresource >= textureDesc.MipLevels) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    bool sharedResource = false;
    D3DTexture3d *textureStaging = nullptr;
    if ((textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED || textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_KEYEDMUTEX) &&
        subresource == 0) {
        textureStaging = d3dTexture;
        sharedResource = true;
    } else {
        sharingFcns->createTexture3d(&textureStaging, &textureDesc, subresource);
    }

    GraphicsAllocation *alloc = nullptr;
    if (textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE) {
        sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle);
        alloc = context->getMemoryManager()->createGraphicsAllocationFromNTHandle(sharedHandle);
    } else {
        sharingFcns->getSharedHandle(textureStaging, &sharedHandle);
        alloc = context->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)((UINT_PTR)sharedHandle), false);
    }
    DEBUG_BREAK_IF(!alloc);

    updateImgInfo(alloc->getDefaultGmm(), imgInfo, imgDesc, OCLPlane::NO_PLANE, 0u);

    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);

    imgInfo.qPitch = alloc->getDefaultGmm()->queryQPitch(GMM_RESOURCE_TYPE::RESOURCE_3D);

    imgInfo.surfaceFormat = findSurfaceFormatInfo(alloc->getDefaultGmm()->gmmResourceInfo->getResourceFormat(), flags);

    if (alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        alloc->getDefaultGmm()->isRenderCompressed = context->getMemoryManager()->mapAuxGpuVA(alloc);
    }

    return Image::createSharedImage(context, d3dTextureObj, mcsSurfaceInfo, alloc, nullptr, flags, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
}

template <typename D3D>
const SurfaceFormatInfo *D3DTexture<D3D>::findYuvSurfaceFormatInfo(DXGI_FORMAT dxgiFormat, OCLPlane oclPlane, cl_mem_flags flags) {
    cl_image_format imgFormat = {};
    if (oclPlane == OCLPlane::PLANE_Y) {
        imgFormat.image_channel_order = CL_R;
    } else {
        imgFormat.image_channel_order = CL_RG;
    }
    if ((dxgiFormat == DXGI_FORMAT_P010) || (dxgiFormat == DXGI_FORMAT_P016)) {
        imgFormat.image_channel_data_type = CL_UNORM_INT16;
    } else {
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
    }

    return Image::getSurfaceFormatFromTable(flags, &imgFormat);
}
