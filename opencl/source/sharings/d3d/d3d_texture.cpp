/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/d3d/d3d_texture.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/mem_obj/image.h"

using namespace NEO;

template <typename D3D>
Image *D3DTexture<D3D>::create2d(Context *context, D3DTexture2d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    ImagePlane imagePlane = ImagePlane::noPlane;
    void *sharedHandle = nullptr;
    cl_uint arrayIndex = 0u;
    McsSurfaceInfo mcsSurfaceInfo = {};
    ImageInfo imgInfo = {};
    imgInfo.imgDesc.imageType = ImageType::image2D;

    D3DTexture2dDesc textureDesc = {};
    sharingFcns->getTexture2dDesc(&textureDesc, d3dTexture);

    cl_int formatSupportError = sharingFcns->validateFormatSupport(textureDesc.Format, CL_MEM_OBJECT_IMAGE2D);
    if (formatSupportError != CL_SUCCESS) {
        err.set(formatSupportError);
        return nullptr;
    }

    if (D3DSharing<D3D>::isFormatWithPlane1(textureDesc.Format)) {
        if ((subresource % 2) == 0) {
            imagePlane = ImagePlane::planeY;
        } else {
            imagePlane = ImagePlane::planeUV;
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

    bool ntHandle = textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE;
    ntHandle ? sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle) : sharingFcns->getSharedHandle(textureStaging, &sharedHandle);
    AllocationProperties allocProperties(rootDeviceIndex,
                                         false, // allocateMemory
                                         0u,    // size
                                         AllocationType::sharedImage,
                                         false, // isMultiStorageAllocation
                                         context->getDeviceBitfieldForAllocation(rootDeviceIndex));
    if (memoryManager->verifyHandle(toOsHandle(sharedHandle), rootDeviceIndex, ntHandle)) {
        MemoryManager::OsHandleData osHandleData{sharedHandle, arrayIndex};
        alloc = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, allocProperties, false, false, true, nullptr);
    } else {
        err.set(CL_INVALID_D3D11_RESOURCE_KHR);
        return nullptr;
    }

    if (alloc == nullptr) {
        err.set(CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }

    D3DSharing<D3D>::updateImgInfoAndDesc(alloc->getDefaultGmm(), imgInfo, imagePlane, arrayIndex);

    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);

    auto &executionEnvironment = memoryManager->peekExecutionEnvironment();
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    const ClSurfaceFormatInfo *clSurfaceFormat = nullptr;
    if ((textureDesc.Format == DXGI_FORMAT_NV12) || (textureDesc.Format == DXGI_FORMAT_P010) || (textureDesc.Format == DXGI_FORMAT_P016)) {
        clSurfaceFormat = findYuvSurfaceFormatInfo(textureDesc.Format, imagePlane, flags);
        imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;
    } else {
        clSurfaceFormat = D3DSharing<D3D>::findSurfaceFormatInfo(alloc->getDefaultGmm()->gmmResourceInfo->getResourceFormat(), flags, hwInfo->capabilityTable.supportsOcl21Features, gfxCoreHelper.packedFormatsSupported());
        imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;
    }

    if (alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        alloc->getDefaultGmm()->setCompressionEnabled(productHelper.isPageTableManagerSupported(*hwInfo) ? memoryManager->mapAuxGpuVA(alloc)
                                                                                                         : true);
    }
    auto multiGraphicsAllocation = MultiGraphicsAllocation(rootDeviceIndex);
    multiGraphicsAllocation.addAllocation(alloc);

    return Image::createSharedImage(context, d3dTextureObj, mcsSurfaceInfo, std::move(multiGraphicsAllocation), nullptr, flags, 0, clSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0, 0, false);
}

template <typename D3D>
Image *D3DTexture<D3D>::create3d(Context *context, D3DTexture3d *d3dTexture, cl_mem_flags flags, cl_uint subresource, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
    void *sharedHandle = nullptr;
    McsSurfaceInfo mcsSurfaceInfo = {};
    ImageInfo imgInfo = {};
    imgInfo.imgDesc.imageType = ImageType::image3D;

    D3DTexture3dDesc textureDesc = {};
    sharingFcns->getTexture3dDesc(&textureDesc, d3dTexture);

    cl_int formatSupportError = sharingFcns->validateFormatSupport(textureDesc.Format, CL_MEM_OBJECT_IMAGE3D);
    if (formatSupportError != CL_SUCCESS) {
        err.set(formatSupportError);
        return nullptr;
    }

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

    bool ntHandle = textureDesc.MiscFlags & D3DResourceFlags::MISC_SHARED_NTHANDLE;
    ntHandle ? sharingFcns->getSharedNTHandle(textureStaging, &sharedHandle) : sharingFcns->getSharedHandle(textureStaging, &sharedHandle);
    AllocationProperties allocProperties(rootDeviceIndex,
                                         false, // allocateMemory
                                         0u,    // size
                                         AllocationType::sharedImage,
                                         false, // isMultiStorageAllocation
                                         context->getDeviceBitfieldForAllocation(rootDeviceIndex));
    if (memoryManager->verifyHandle(toOsHandle(sharedHandle), rootDeviceIndex, ntHandle)) {
        MemoryManager::OsHandleData osHandleData{sharedHandle};
        alloc = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, allocProperties, false, false, true, nullptr);
    } else {
        err.set(CL_INVALID_D3D11_RESOURCE_KHR);
        return nullptr;
    }

    bool is3DUavOrRtv = false;
    if (textureDesc.BindFlags & D3DBindFLags::D3D11_BIND_RENDER_TARGET || textureDesc.BindFlags & D3DBindFLags::D3D11_BIND_UNORDERED_ACCESS) {
        is3DUavOrRtv = true;
    }
    if (alloc == nullptr) {
        err.set(CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }

    D3DSharing<D3D>::updateImgInfoAndDesc(alloc->getDefaultGmm(), imgInfo, ImagePlane::noPlane, 0u);

    auto &executionEnvironment = memoryManager->peekExecutionEnvironment();
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto d3dTextureObj = new D3DTexture<D3D>(context, d3dTexture, subresource, textureStaging, sharedResource);
    auto *clSurfaceFormat = D3DSharing<D3D>::findSurfaceFormatInfo(alloc->getDefaultGmm()->gmmResourceInfo->getResourceFormat(), flags, hwInfo->capabilityTable.supportsOcl21Features, gfxCoreHelper.packedFormatsSupported());
    imgInfo.qPitch = alloc->getDefaultGmm()->queryQPitch(GMM_RESOURCE_TYPE::RESOURCE_3D);

    imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;

    if (alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        alloc->getDefaultGmm()->setCompressionEnabled(productHelper.isPageTableManagerSupported(*hwInfo) ? memoryManager->mapAuxGpuVA(alloc)
                                                                                                         : true);
    }
    auto multiGraphicsAllocation = MultiGraphicsAllocation(rootDeviceIndex);
    multiGraphicsAllocation.addAllocation(alloc);

    auto image = Image::createSharedImage(context, d3dTextureObj, mcsSurfaceInfo, std::move(multiGraphicsAllocation), nullptr, flags, 0, clSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0, 0, false);
    image->setAs3DUavOrRtvImage(is3DUavOrRtv);
    return image;
}

template <typename D3D>
const ClSurfaceFormatInfo *D3DTexture<D3D>::findYuvSurfaceFormatInfo(DXGI_FORMAT dxgiFormat, ImagePlane imagePlane, cl_mem_flags flags) {
    cl_image_format imgFormat = {};
    if (imagePlane == ImagePlane::planeY) {
        imgFormat.image_channel_order = CL_R;
    } else {
        imgFormat.image_channel_order = CL_RG;
    }
    if ((dxgiFormat == DXGI_FORMAT_P010) || (dxgiFormat == DXGI_FORMAT_P016)) {
        imgFormat.image_channel_data_type = CL_UNORM_INT16;
    } else {
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
    }

    return Image::getSurfaceFormatFromTable(flags, &imgFormat, false /* supportsOcl20Features */);
}

template class NEO::D3DTexture<D3DTypesHelper::D3D10>;
template class NEO::D3DTexture<D3DTypesHelper::D3D11>;
