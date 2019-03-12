/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/va/va_surface.h"

#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/get_info.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
Image *VASurface::createSharedVaSurface(Context *context, VASharingFunctions *sharingFunctions,
                                        cl_mem_flags flags, VASurfaceID *surface,
                                        cl_uint plane, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    auto memoryManager = context->getMemoryManager();
    unsigned int sharedHandle = 0;
    VAImage vaImage = {};
    cl_image_desc imgDesc = {};
    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    cl_image_format imgFormat = {};
    const SurfaceFormatInfo *gmmSurfaceFormat = nullptr;
    const SurfaceFormatInfo *imgSurfaceFormat = nullptr;
    ImageInfo imgInfo = {0};
    VAImageID imageId = 0;
    McsSurfaceInfo mcsSurfaceInfo = {};

    sharingFunctions->deriveImage(*surface, &vaImage);

    imageId = vaImage.image_id;
    imgInfo.imgDesc = &imgDesc;
    imgDesc.image_width = vaImage.width;
    imgDesc.image_height = vaImage.height;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat);
    imgInfo.surfaceFormat = gmmSurfaceFormat;

    if (plane == 0) {
        imgInfo.plane = GMM_PLANE_Y;
        imgFormat = {CL_R, CL_UNORM_INT8};
    } else if (plane == 1) {
        imgInfo.plane = GMM_PLANE_U;
        imgFormat = {CL_RG, CL_UNORM_INT8};
    } else {
        imgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
        imgInfo.plane = GMM_NO_PLANE;
    }

    imgSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &imgFormat);

    sharingFunctions->extGetSurfaceHandle(surface, &sharedHandle);

    auto alloc = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, false);

    Gmm *gmm = new Gmm(imgInfo);
    DEBUG_BREAK_IF(alloc->getDefaultGmm());
    alloc->setDefaultGmm(gmm);

    imgDesc.image_row_pitch = imgInfo.rowPitch;
    imgDesc.image_slice_pitch = 0u;
    imgInfo.slicePitch = 0u;
    imgInfo.surfaceFormat = imgSurfaceFormat;
    if (plane == 1) {
        imgDesc.image_width /= 2;
        imgDesc.image_height /= 2;
        imgInfo.offset = vaImage.offsets[1];
        imgInfo.yOffset = 0;
        imgInfo.xOffset = 0;
        imgInfo.yOffsetForUVPlane = static_cast<uint32_t>(imgInfo.offset / vaImage.pitches[0]);
    }
    sharingFunctions->destroyImage(vaImage.image_id);

    auto vaSurface = new VASurface(sharingFunctions, imageId, plane, surface, context->getInteropUserSyncEnabled());

    auto image = Image::createSharedImage(context, vaSurface, mcsSurfaceInfo, alloc, nullptr, flags, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
    image->setMediaPlaneType(plane);
    return image;
}

void VASurface::synchronizeObject(UpdateData &updateData) {
    if (!interopUserSync) {
        sharingFunctions->syncSurface(*surfaceId);
    }
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
}

void VASurface::getMemObjectInfo(size_t &paramValueSize, void *&paramValue) {
    paramValueSize = sizeof(surfaceId);
    paramValue = &surfaceId;
}

} // namespace OCLRT
