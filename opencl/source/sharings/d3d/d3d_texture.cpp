/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/d3d/d3d_texture.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_types_converter.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "opencl/source/context/context.h"
#include "opencl/source/device/cl_device.h"
#include "opencl/source/mem_obj/image.h"

using namespace NEO;

template class D3DTexture<D3DTypesHelper::D3D10>;
template class D3DTexture<D3DTypesHelper::D3D11>;

template <typename D3D>
Image *D3DTexture<D3D>::create2d(Context *context, D3DTexture2d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    ImagePlane imagePlane = ImagePlane::NO_PLANE;
    void *sharedHandle = nullptr;
    cl_uint arrayIndex = 0u;
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};
    ImageInfo imgInfo = {};
    imgInfo.imgDesc.imageType = ImageType::Image2D;

    D3DTexture2dDesc textureDesc = {};
    sharingFcns->getTexture2dDesc(&textureDesc, d3dTexture);

    if ((textureDesc.Format == DXGI_FORMAT_NV12) || (textureDesc.Format == DXGI_FORMAT_P010) || (textureDesc.Format == DXGI_FORMAT_P016)) {
        if ((subresource % 2) == 0) {
            imagePlane = ImagePlane::PLANE_Y;
        } else {
            imagePlane = ImagePlane::PLANE_UV;
        }
        imgInfo.plane = GmmTypesConverter::convertPlane(imagePlane);
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
    auto memoryManager = context->getMemoryManager();
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();

    if (textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE) {
        sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle);
        alloc = memoryManager->createGraphicsAllocationFromNTHandle(sharedHandle, rootDeviceIndex);
    } else {
        sharingFcns->getSharedHandle(textureStaging, &sharedHandle);
        AllocationProperties allocProperties(rootDeviceIndex, nullptr, false, GraphicsAllocation::AllocationType::SHARED_IMAGE, false);
        alloc = memoryManager->createGraphicsAllocationFromSharedHandle(toOsHandle(sharedHandle), allocProperties, false);
    }
    DEBUG_BREAK_IF(!alloc);

    updateImgInfoAndDesc(alloc->getDefaultGmm(), imgInfo, imagePlane, arrayIndex);

    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);

    const ClSurfaceFormatInfo *clSurfaceFormat = nullptr;
    if ((textureDesc.Format == DXGI_FORMAT_NV12) || (textureDesc.Format == DXGI_FORMAT_P010) || (textureDesc.Format == DXGI_FORMAT_P016)) {
        clSurfaceFormat = findYuvSurfaceFormatInfo(textureDesc.Format, imagePlane, flags);
        imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;
    } else {
        clSurfaceFormat = findSurfaceFormatInfo(alloc->getDefaultGmm()->gmmResourceInfo->getResourceFormat(), flags, context->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
        imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;
    }

    auto hwInfo = memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        alloc->getDefaultGmm()->isRenderCompressed = hwHelper.isPageTableManagerSupported(*hwInfo) ? memoryManager->mapAuxGpuVA(alloc)
                                                                                                   : true;
    }

    return Image::createSharedImage(context, d3dTextureObj, mcsSurfaceInfo, alloc, nullptr, flags, clSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
}

template <typename D3D>
Image *D3DTexture<D3D>::create3d(Context *context, D3DTexture3d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    void *sharedHandle = nullptr;
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};
    ImageInfo imgInfo = {};
    imgInfo.imgDesc.imageType = ImageType::Image3D;

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
    auto memoryManager = context->getMemoryManager();
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();

    if (textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE) {
        sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle);
        alloc = memoryManager->createGraphicsAllocationFromNTHandle(sharedHandle, rootDeviceIndex);
    } else {
        sharingFcns->getSharedHandle(textureStaging, &sharedHandle);
        AllocationProperties allocProperties(rootDeviceIndex, nullptr, false, GraphicsAllocation::AllocationType::SHARED_IMAGE, false);
        alloc = memoryManager->createGraphicsAllocationFromSharedHandle(toOsHandle(sharedHandle), allocProperties, false);
    }
    DEBUG_BREAK_IF(!alloc);

    updateImgInfoAndDesc(alloc->getDefaultGmm(), imgInfo, ImagePlane::NO_PLANE, 0u);

    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);
    auto *clSurfaceFormat = findSurfaceFormatInfo(alloc->getDefaultGmm()->gmmResourceInfo->getResourceFormat(), flags, context->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
    imgInfo.qPitch = alloc->getDefaultGmm()->queryQPitch(GMM_RESOURCE_TYPE::RESOURCE_3D);

    imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;

    auto hwInfo = memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        alloc->getDefaultGmm()->isRenderCompressed = hwHelper.isPageTableManagerSupported(*hwInfo) ? memoryManager->mapAuxGpuVA(alloc)
                                                                                                   : true;
    }

    return Image::createSharedImage(context, d3dTextureObj, mcsSurfaceInfo, alloc, nullptr, flags, clSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
}

template <typename D3D>
const ClSurfaceFormatInfo *D3DTexture<D3D>::findYuvSurfaceFormatInfo(DXGI_FORMAT dxgiFormat, ImagePlane imagePlane, cl_mem_flags flags) {
    cl_image_format imgFormat = {};
    if (imagePlane == ImagePlane::PLANE_Y) {
        imgFormat.image_channel_order = CL_R;
    } else {
        imgFormat.image_channel_order = CL_RG;
    }
    if ((dxgiFormat == DXGI_FORMAT_P010) || (dxgiFormat == DXGI_FORMAT_P016)) {
        imgFormat.image_channel_data_type = CL_UNORM_INT16;
    } else {
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
    }

    return Image::getSurfaceFormatFromTable(flags, &imgFormat, 12);
}
