/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/d3d/d3d_surface.h"

#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/memory_manager/memory_manager.h"

#include "mmsystem.h"

using namespace NEO;

D3DSurface::D3DSurface(Context *context, cl_dx9_surface_info_khr *surfaceInfo, D3D9Surface *surfaceStaging, cl_uint plane,
                       OCLPlane oclPlane, cl_dx9_media_adapter_type_khr adapterType, bool sharedResource, bool lockable)
    : D3DSharing(context, surfaceInfo->resource, surfaceStaging, plane, sharedResource), adapterType(adapterType),
      surfaceInfo(*surfaceInfo), lockable(lockable), plane(plane), oclPlane(oclPlane), d3d9Surface(surfaceInfo->resource),
      d3d9SurfaceStaging(surfaceStaging) {
    if (sharingFunctions) {
        resourceDevice = sharingFunctions->getDevice();
    }
};

Image *D3DSurface::create(Context *context, cl_dx9_surface_info_khr *surfaceInfo, cl_mem_flags flags,
                          cl_dx9_media_adapter_type_khr adapterType, cl_uint plane, cl_int *retCode) {
    ErrorCodeHelper err(retCode, CL_SUCCESS);
    D3D9Surface *surfaceStaging = nullptr;
    cl_image_desc imgDesc = {};
    ImageInfo imgInfo = {};
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};
    OCLPlane oclPlane = OCLPlane::NO_PLANE;

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

    imgInfo.imgDesc = &imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    D3D9SurfaceDesc surfaceDesc = {};
    sharingFcns->getTexture2dDesc(&surfaceDesc, surfaceInfo->resource);
    imgDesc.image_width = surfaceDesc.Width;
    imgDesc.image_height = surfaceDesc.Height;

    if (surfaceDesc.Pool != D3DPOOL_DEFAULT) {
        err.set(CL_INVALID_DX9_RESOURCE_INTEL);
        return nullptr;
    }

    err.set(findImgFormat(surfaceDesc.Format, imgFormat, plane, oclPlane));
    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    imgInfo.plane = GmmHelper::convertPlane(oclPlane);
    imgInfo.surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imgFormat);

    bool isSharedResource = false;
    bool lockable = false;
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();

    GraphicsAllocation *alloc = nullptr;
    if (surfaceInfo->shared_handle) {
        isSharedResource = true;
        AllocationProperties allocProperties(rootDeviceIndex, false, 0u, GraphicsAllocation::AllocationType::SHARED_IMAGE, false);
        alloc = context->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)((UINT_PTR)surfaceInfo->shared_handle), allocProperties,
                                                                                      false);
        updateImgInfo(alloc->getDefaultGmm(), imgInfo, imgDesc, oclPlane, 0u);
    } else {
        lockable = !(surfaceDesc.Usage & D3DResourceFlags::USAGE_RENDERTARGET) || oclPlane != OCLPlane::NO_PLANE;
        if (!lockable) {
            sharingFcns->createTexture2d(&surfaceStaging, &surfaceDesc, 0u);
        }
        if (oclPlane == OCLPlane::PLANE_U || oclPlane == OCLPlane::PLANE_V || oclPlane == OCLPlane::PLANE_UV) {
            imgDesc.image_width /= 2;
            imgDesc.image_height /= 2;
        }
        MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0);
        AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(rootDeviceIndex, imgInfo, true, memoryProperties);
        allocProperties.allocationType = GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY;

        alloc = context->getMemoryManager()->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);

        imgDesc.image_row_pitch = imgInfo.rowPitch;
        imgDesc.image_slice_pitch = imgInfo.slicePitch;
    }
    DEBUG_BREAK_IF(!alloc);

    auto surface = new D3DSurface(context, surfaceInfo, surfaceStaging, plane, oclPlane, adapterType, isSharedResource, lockable);

    return Image::createSharedImage(context, surface, mcsSurfaceInfo, alloc, nullptr, flags, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
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
        auto sys = lockedRect.pBits;
        auto gpu = context->getMemoryManager()->lockResource(image->getGraphicsAllocation());
        auto pitch = static_cast<ULONG>(lockedRect.Pitch);
        auto height = static_cast<ULONG>(image->getImageDesc().image_height);

        image->getGraphicsAllocation()->getDefaultGmm()->resourceCopyBlt(sys, gpu, pitch, height, 1u, oclPlane);

        context->getMemoryManager()->unlockResource(updateData.memObject->getGraphicsAllocation());

        if (lockable) {
            sharingFunctions->unlockRect(d3d9Surface);
        } else {
            sharingFunctions->unlockRect(d3d9SurfaceStaging);
        }

        sharingFunctions->flushAndWait(d3dQuery);
    }
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
}

void D3DSurface::releaseResource(MemObj *memObject) {
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
        auto gpu = context->getMemoryManager()->lockResource(image->getGraphicsAllocation());
        auto pitch = static_cast<ULONG>(lockedRect.Pitch);
        auto height = static_cast<ULONG>(image->getImageDesc().image_height);

        image->getGraphicsAllocation()->getDefaultGmm()->resourceCopyBlt(sys, gpu, pitch, height, 0u, oclPlane);

        context->getMemoryManager()->unlockResource(memObject->getGraphicsAllocation());

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

cl_int D3DSurface::findImgFormat(D3DFORMAT d3dFormat, cl_image_format &imgFormat, cl_uint plane, OCLPlane &oclPlane) {
    oclPlane = OCLPlane::NO_PLANE;
    static const cl_image_format unknown_format = {0, 0};

    auto element = D3DtoClFormatConversions.find(d3dFormat);
    if (element == D3DtoClFormatConversions.end()) {
        imgFormat = unknown_format;
        return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
    }

    imgFormat = element->second;
    switch (d3dFormat) {
    case static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')):
        switch (plane) {
        case 0:
            imgFormat.image_channel_order = CL_R;
            oclPlane = OCLPlane::PLANE_Y;
            return CL_SUCCESS;
        case 1:
            imgFormat.image_channel_order = CL_RG;
            oclPlane = OCLPlane::PLANE_UV;
            return CL_SUCCESS;
        default:
            imgFormat = unknown_format;
            return CL_INVALID_VALUE;
        }

    case static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', '1', '2')):
        switch (plane) {
        case 0:
            oclPlane = OCLPlane::PLANE_Y;
            return CL_SUCCESS;

        case 1:
            oclPlane = OCLPlane::PLANE_V;
            return CL_SUCCESS;

        case 2:
            oclPlane = OCLPlane::PLANE_U;
            return CL_SUCCESS;

        default:
            imgFormat = unknown_format;
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
