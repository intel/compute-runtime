/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/d3d/d3d_surface.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"

#include "mmsystem.h"

using namespace NEO;

D3DSurface::D3DSurface(Context *context, cl_dx9_surface_info_khr *surfaceInfo, D3D9Surface *surfaceStaging, cl_uint plane,
                       ImagePlane imagePlane, cl_dx9_media_adapter_type_khr adapterType, bool sharedResource, bool lockable)
    : D3DSharing(context, surfaceInfo->resource, surfaceStaging, plane, sharedResource), lockable(lockable), adapterType(adapterType),
      surfaceInfo(*surfaceInfo), plane(plane), imagePlane(imagePlane), d3d9Surface(surfaceInfo->resource),
      d3d9SurfaceStaging(surfaceStaging) {
    if (sharingFunctions) {
        resourceDevice = sharingFunctions->getDevice();
    }
};

Image *D3DSurface::create(Context *context, cl_dx9_surface_info_khr *surfaceInfo, cl_mem_flags flags,
                          cl_dx9_media_adapter_type_khr adapterType, cl_uint plane, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    D3D9Surface *surfaceStaging = nullptr;
    ImageInfo imgInfo = {};
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};
    ImagePlane imagePlane = ImagePlane::noPlane;

    if (!context || !context->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D9>>() || !context->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D9>>()->getDevice()) {
        err.set(CL_INVALID_CONTEXT);
        return nullptr;
    }
    auto sharingFcns = context->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D9>>();
    if (sharingFcns->isTracked(surfaceInfo->resource, plane)) {
        err.set(CL_INVALID_DX9_RESOURCE_INTEL);
        return nullptr;
    }

    sharingFcns->updateDevice(surfaceInfo->resource);

    imgInfo.imgDesc.imageType = ImageType::image2D;

    D3D9SurfaceDesc surfaceDesc = {};
    sharingFcns->getTexture2dDesc(&surfaceDesc, surfaceInfo->resource);
    imgInfo.imgDesc.imageWidth = surfaceDesc.Width;
    imgInfo.imgDesc.imageHeight = surfaceDesc.Height;

    if (surfaceDesc.Pool != D3DPOOL_DEFAULT) {
        err.set(CL_INVALID_DX9_RESOURCE_INTEL);
        return nullptr;
    }

    err.set(findImgFormat(surfaceDesc.Format, imgFormat, plane, imagePlane));
    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    imgInfo.plane = GmmTypesConverter::convertPlane(imagePlane);
    auto *clSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;

    bool isSharedResource = false;
    bool lockable = false;
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();

    GraphicsAllocation *alloc = nullptr;
    if (surfaceInfo->shared_handle) {
        isSharedResource = true;
        AllocationProperties allocProperties(rootDeviceIndex,
                                             false, // allocateMemory
                                             0u,    // size
                                             AllocationType::sharedImage,
                                             false, // isMultiStorageAllocation
                                             context->getDeviceBitfieldForAllocation(rootDeviceIndex));
        MemoryManager::OsHandleData osHandleData{surfaceInfo->shared_handle};
        alloc = context->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, allocProperties,
                                                                                      false, false, true, nullptr);
        updateImgInfoAndDesc(alloc->getDefaultGmm(), imgInfo, imagePlane, 0u);
    } else {
        lockable = !(surfaceDesc.Usage & D3DResourceFlags::USAGE_RENDERTARGET) || imagePlane != ImagePlane::noPlane;
        if (!lockable) {
            sharingFcns->createTexture2d(&surfaceStaging, &surfaceDesc, 0u);
        }
        if (imagePlane == ImagePlane::planeU || imagePlane == ImagePlane::planeV || imagePlane == ImagePlane::planeUV) {
            imgInfo.imgDesc.imageWidth /= 2;
            imgInfo.imgDesc.imageHeight /= 2;
        }
        MemoryProperties memoryProperties =
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice());
        AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(rootDeviceIndex, imgInfo,
                                                                                                  true, // allocateMemory
                                                                                                  memoryProperties, context->getDevice(0)->getHardwareInfo(),
                                                                                                  context->getDeviceBitfieldForAllocation(rootDeviceIndex),
                                                                                                  context->isSingleDeviceContext());
        allocProperties.allocationType = AllocationType::sharedResourceCopy;

        alloc = context->getMemoryManager()->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);

        imgInfo.imgDesc.imageRowPitch = imgInfo.rowPitch;
        imgInfo.imgDesc.imageSlicePitch = imgInfo.slicePitch;
    }
    DEBUG_BREAK_IF(!alloc);

    auto surface = new D3DSurface(context, surfaceInfo, surfaceStaging, plane, imagePlane, adapterType, isSharedResource, lockable);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(rootDeviceIndex);
    multiGraphicsAllocation.addAllocation(alloc);

    return Image::createSharedImage(context, surface, mcsSurfaceInfo, std::move(multiGraphicsAllocation), nullptr, flags, 0, clSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0, 0, false);
}

void D3DSurface::synchronizeObject(UpdateData &updateData) {
    D3DLOCKED_RECT lockedRect = {};
    sharingFunctions->setDevice(resourceDevice);
    if (sharedResource && !context->getInteropUserSyncEnabled()) {
        sharingFunctions->flushAndWait(d3dQuery);
    } else if (!sharedResource) {
        if (lockable) {
            sharingFunctions->lockRect(d3d9Surface, &lockedRect, D3DLOCK_READONLY);
        } else {
            sharingFunctions->getRenderTargetData(d3d9Surface, d3d9SurfaceStaging);
            sharingFunctions->lockRect(d3d9SurfaceStaging, &lockedRect, D3DLOCK_READONLY);
        }

        auto image = castToObjectOrAbort<Image>(updateData.memObject);
        auto graphicsAllocation = image->getGraphicsAllocation(updateData.rootDeviceIndex);
        auto sys = lockedRect.pBits;
        auto gpu = context->getMemoryManager()->lockResource(graphicsAllocation);
        auto pitch = static_cast<ULONG>(lockedRect.Pitch);
        auto height = static_cast<ULONG>(image->getImageDesc().image_height);

        graphicsAllocation->getDefaultGmm()->resourceCopyBlt(sys, gpu, pitch, height, 1u, imagePlane);

        context->getMemoryManager()->unlockResource(graphicsAllocation);

        if (lockable) {
            sharingFunctions->unlockRect(d3d9Surface);
        } else {
            sharingFunctions->unlockRect(d3d9SurfaceStaging);
        }

        sharingFunctions->flushAndWait(d3dQuery);
    }
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
}

void D3DSurface::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
    D3DLOCKED_RECT lockedRect = {};
    auto image = castToObject<Image>(memObject);
    if (!image) {
        return;
    }

    sharingFunctions->setDevice(resourceDevice);
    if (!sharedResource) {
        if (lockable) {
            sharingFunctions->lockRect(d3d9Surface, &lockedRect, 0);
        } else {
            sharingFunctions->lockRect(d3d9SurfaceStaging, &lockedRect, 0);
        }

        auto sys = lockedRect.pBits;
        auto graphicsAllocation = image->getGraphicsAllocation(rootDeviceIndex);
        auto gpu = context->getMemoryManager()->lockResource(graphicsAllocation);
        auto pitch = static_cast<ULONG>(lockedRect.Pitch);
        auto height = static_cast<ULONG>(image->getImageDesc().image_height);

        graphicsAllocation->getDefaultGmm()->resourceCopyBlt(sys, gpu, pitch, height, 0u, imagePlane);

        context->getMemoryManager()->unlockResource(graphicsAllocation);

        if (lockable) {
            sharingFunctions->unlockRect(d3d9Surface);
        } else {
            sharingFunctions->unlockRect(d3d9SurfaceStaging);
            sharingFunctions->updateSurface(d3d9SurfaceStaging, d3d9Surface);
        }
    }
}

const std::map<const D3DFORMAT, const cl_image_format> D3DSurface::D3DtoClFormatConversions = {
    {D3DFMT_R32F, {CL_R, CL_FLOAT}},
    {D3DFMT_R16F, {CL_R, CL_HALF_FLOAT}},
    {D3DFMT_L16, {CL_R, CL_UNORM_INT16}},
    {D3DFMT_A8, {CL_A, CL_UNORM_INT8}},
    {D3DFMT_L8, {CL_R, CL_UNORM_INT8}},
    {D3DFMT_G32R32F, {CL_RG, CL_FLOAT}},
    {D3DFMT_G16R16F, {CL_RG, CL_HALF_FLOAT}},
    {D3DFMT_G16R16, {CL_RG, CL_UNORM_INT16}},
    {D3DFMT_A8L8, {CL_RG, CL_UNORM_INT8}},
    {D3DFMT_A32B32G32R32F, {CL_RGBA, CL_FLOAT}},
    {D3DFMT_A16B16G16R16F, {CL_RGBA, CL_HALF_FLOAT}},
    {D3DFMT_A16B16G16R16, {CL_RGBA, CL_UNORM_INT16}},
    {D3DFMT_X8B8G8R8, {CL_RGBA, CL_UNORM_INT8}},
    {D3DFMT_A8B8G8R8, {CL_RGBA, CL_UNORM_INT8}},
    {D3DFMT_A8R8G8B8, {CL_BGRA, CL_UNORM_INT8}},
    {D3DFMT_X8R8G8B8, {CL_BGRA, CL_UNORM_INT8}},
    {D3DFMT_YUY2, {CL_YUYV_INTEL, CL_UNORM_INT8}},
    {D3DFMT_UYVY, {CL_UYVY_INTEL, CL_UNORM_INT8}},

    // The specific channel_order for NV12 is selected in findImgFormat
    {static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')), {CL_R | CL_RG, CL_UNORM_INT8}},
    {static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', '1', '2')), {CL_R, CL_UNORM_INT8}},
    {static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', 'Y', 'U')), {CL_YVYU_INTEL, CL_UNORM_INT8}},
    {static_cast<D3DFORMAT>(MAKEFOURCC('V', 'Y', 'U', 'Y')), {CL_VYUY_INTEL, CL_UNORM_INT8}}};

const std::vector<D3DFORMAT> D3DSurface::D3DPlane1Formats = {
    static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')),
    static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', '1', '2'))};

const std::vector<D3DFORMAT> D3DSurface::D3DPlane2Formats =
    {static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', '1', '2'))};

cl_int D3DSurface::findImgFormat(D3DFORMAT d3dFormat, cl_image_format &imgFormat, cl_uint plane, ImagePlane &imagePlane) {
    imagePlane = ImagePlane::noPlane;
    static const cl_image_format unknownFormat = {0, 0};

    auto element = D3DtoClFormatConversions.find(d3dFormat);
    if (element == D3DtoClFormatConversions.end()) {
        imgFormat = unknownFormat;
        return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
    }

    imgFormat = element->second;
    int d3dFormatValue = d3dFormat;
    switch (d3dFormatValue) {
    case MAKEFOURCC('N', 'V', '1', '2'):
        switch (plane) {
        case 0:
            imgFormat.image_channel_order = CL_R;
            imagePlane = ImagePlane::planeY;
            return CL_SUCCESS;
        case 1:
            imgFormat.image_channel_order = CL_RG;
            imagePlane = ImagePlane::planeUV;
            return CL_SUCCESS;
        default:
            imgFormat = unknownFormat;
            return CL_INVALID_VALUE;
        }

    case MAKEFOURCC('Y', 'V', '1', '2'):
        switch (plane) {
        case 0:
            imagePlane = ImagePlane::planeY;
            return CL_SUCCESS;

        case 1:
            imagePlane = ImagePlane::planeV;
            return CL_SUCCESS;

        case 2:
            imagePlane = ImagePlane::planeU;
            return CL_SUCCESS;

        default:
            imgFormat = unknownFormat;
            return CL_INVALID_VALUE;
        }
    }

    if (plane > 0) {
        return CL_INVALID_VALUE;
    }
    return CL_SUCCESS;
}

int D3DSurface::validateUpdateData(UpdateData &updateData) {
    auto image = castToObject<Image>(updateData.memObject);
    if (!image) {
        return CL_INVALID_MEM_OBJECT;
    }
    return CL_SUCCESS;
}
