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

#include "runtime/sharings/d3d/d3d_surface.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/mem_obj/image.h"
#include "runtime/helpers/get_info.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/gmm.h"
#include "mmsystem.h"

using namespace OCLRT;

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

    GraphicsAllocation *alloc = nullptr;
    if (surfaceInfo->shared_handle) {
        isSharedResource = true;
        alloc = context->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)((UINT_PTR)surfaceInfo->shared_handle), false);
        updateImgInfo(alloc->gmm, imgInfo, imgDesc, oclPlane, 0u);
    } else {
        lockable = !(surfaceDesc.Usage & D3DResourceFlags::USAGE_RENDERTARGET) || oclPlane != OCLPlane::NO_PLANE;
        if (!lockable) {
            sharingFcns->createTexture2d(&surfaceStaging, &surfaceDesc, 0u);
        }
        if (oclPlane == OCLPlane::PLANE_U || oclPlane == OCLPlane::PLANE_V || oclPlane == OCLPlane::PLANE_UV) {
            imgDesc.image_width /= 2;
            imgDesc.image_height /= 2;
        }
        Gmm *gmm = GmmHelper::createGmmAndQueryImgParams(imgInfo, context->getDevice(0)->getHardwareInfo());
        imgDesc.image_row_pitch = imgInfo.rowPitch;
        imgDesc.image_slice_pitch = imgInfo.slicePitch;

        alloc = context->getMemoryManager()->allocateGraphicsMemoryForImage(imgInfo, gmm);
    }
    DEBUG_BREAK_IF(!alloc);

    auto surface = new D3DSurface(context, surfaceInfo, surfaceStaging, plane, oclPlane, adapterType, isSharedResource, lockable);

    return Image::createSharedImage(context, surface, mcsSurfaceInfo, alloc, nullptr, flags, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
}

void D3DSurface::synchronizeObject(UpdateData *updateData) {
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

        auto image = castToObjectOrAbort<Image>(updateData->memObject);
        auto sys = lockedRect.pBits;
        auto gpu = context->getMemoryManager()->lockResource(image->getGraphicsAllocation());
        auto pitch = static_cast<ULONG>(lockedRect.Pitch);
        auto height = static_cast<ULONG>(image->getImageDesc().image_height);

        image->getGraphicsAllocation()->gmm->resourceCopyBlt(sys, gpu, pitch, height, 1u, oclPlane);

        context->getMemoryManager()->unlockResource(updateData->memObject->getGraphicsAllocation());

        if (lockable) {
            sharingFunctions->unlockRect(d3d9Surface);
        } else {
            sharingFunctions->unlockRect(d3d9SurfaceStaging);
        }

        sharingFunctions->flushAndWait(d3dQuery);
    }
    updateData->synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
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

        image->getGraphicsAllocation()->gmm->resourceCopyBlt(sys, gpu, pitch, height, 0u, oclPlane);

        context->getMemoryManager()->unlockResource(memObject->getGraphicsAllocation());

        if (lockable) {
            sharingFunctions->unlockRect(d3d9Surface);
        } else {
            sharingFunctions->unlockRect(d3d9SurfaceStaging);
            sharingFunctions->updateSurface(d3d9SurfaceStaging, d3d9Surface);
        }
    }
}

cl_int D3DSurface::findImgFormat(D3DFORMAT d3dFormat, cl_image_format &imgFormat, cl_uint plane, OCLPlane &oclPlane) {
    oclPlane = OCLPlane::NO_PLANE;
    bool noPlaneRequired = true;
    switch (d3dFormat) {
    case D3DFMT_R32F:
        imgFormat.image_channel_order = CL_R;
        imgFormat.image_channel_data_type = CL_FLOAT;
        break;
    case D3DFMT_R16F:
        imgFormat.image_channel_order = CL_R;
        imgFormat.image_channel_data_type = CL_HALF_FLOAT;
        break;
    case D3DFMT_L16:
        imgFormat.image_channel_order = CL_R;
        imgFormat.image_channel_data_type = CL_UNORM_INT16;
        break;
    case D3DFMT_A8:
        imgFormat.image_channel_order = CL_A;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case D3DFMT_L8:
        imgFormat.image_channel_order = CL_R;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case D3DFMT_G32R32F:
        imgFormat.image_channel_order = CL_RG;
        imgFormat.image_channel_data_type = CL_FLOAT;
        break;
    case D3DFMT_G16R16F:
        imgFormat.image_channel_order = CL_RG;
        imgFormat.image_channel_data_type = CL_HALF_FLOAT;
        break;
    case D3DFMT_G16R16:
        imgFormat.image_channel_order = CL_RG;
        imgFormat.image_channel_data_type = CL_UNORM_INT16;
        break;
    case D3DFMT_A8L8:
        imgFormat.image_channel_order = CL_RG;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case D3DFMT_A32B32G32R32F:
        imgFormat.image_channel_order = CL_RGBA;
        imgFormat.image_channel_data_type = CL_FLOAT;
        break;
    case D3DFMT_A16B16G16R16F:
        imgFormat.image_channel_order = CL_RGBA;
        imgFormat.image_channel_data_type = CL_HALF_FLOAT;
        break;
    case D3DFMT_A16B16G16R16:
        imgFormat.image_channel_order = CL_RGBA;
        imgFormat.image_channel_data_type = CL_UNORM_INT16;
        break;
    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
        imgFormat.image_channel_order = CL_RGBA;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
        imgFormat.image_channel_order = CL_BGRA;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'):
        switch (plane) {
        case 0:
            imgFormat.image_channel_order = CL_R;
            imgFormat.image_channel_data_type = CL_UNORM_INT8;
            oclPlane = OCLPlane::PLANE_Y;
            break;
        case 1:
            imgFormat.image_channel_order = CL_RG;
            imgFormat.image_channel_data_type = CL_UNORM_INT8;
            oclPlane = OCLPlane::PLANE_UV;
            break;
        default:
            imgFormat.image_channel_order = 0;
            imgFormat.image_channel_data_type = 0;
            return CL_INVALID_VALUE;
        }
        noPlaneRequired = false;
        break;
    case (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'):
        switch (plane) {
        case 0:
            imgFormat.image_channel_order = CL_R;
            imgFormat.image_channel_data_type = CL_UNORM_INT8;
            oclPlane = OCLPlane::PLANE_Y;
            break;
        case 1:
            imgFormat.image_channel_order = CL_R;
            imgFormat.image_channel_data_type = CL_UNORM_INT8;
            oclPlane = OCLPlane::PLANE_V;
            break;
        case 2:
            imgFormat.image_channel_order = CL_R;
            imgFormat.image_channel_data_type = CL_UNORM_INT8;
            oclPlane = OCLPlane::PLANE_U;
            break;
        default:
            imgFormat.image_channel_order = 0;
            imgFormat.image_channel_data_type = 0;
            return CL_INVALID_VALUE;
        }
        noPlaneRequired = false;
        break;
    case D3DFMT_YUY2:
        imgFormat.image_channel_order = CL_YUYV_INTEL;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case D3DFMT_UYVY:
        imgFormat.image_channel_order = CL_UYVY_INTEL;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case (D3DFORMAT)MAKEFOURCC('Y', 'V', 'Y', 'U'):
        imgFormat.image_channel_order = CL_YVYU_INTEL;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    case (D3DFORMAT)MAKEFOURCC('V', 'Y', 'U', 'Y'):
        imgFormat.image_channel_order = CL_VYUY_INTEL;
        imgFormat.image_channel_data_type = CL_UNORM_INT8;
        break;
    default:
        imgFormat.image_channel_order = 0;
        imgFormat.image_channel_data_type = 0;
        return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
    }
    if (noPlaneRequired && plane > 0) {
        return CL_INVALID_VALUE;
    }
    return CL_SUCCESS;
}

int D3DSurface::validateUpdateData(UpdateData *updateData) {
    UNRECOVERABLE_IF(updateData == nullptr);
    auto image = castToObject<Image>(updateData->memObject);
    if (!image) {
        return CL_INVALID_MEM_OBJECT;
    }
    return CL_SUCCESS;
}
