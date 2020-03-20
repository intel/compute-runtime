/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/va/va_surface.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"

namespace NEO {
Image *VASurface::createSharedVaSurface(Context *context, VASharingFunctions *sharingFunctions,
                                        cl_mem_flags flags, VASurfaceID *surface,
                                        cl_uint plane, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    auto memoryManager = context->getMemoryManager();
    unsigned int sharedHandle = 0;
    VAImage vaImage = {};
    cl_image_desc imgDesc = {};
    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    cl_channel_order channelOrder = CL_RG;
    cl_channel_type channelType = CL_UNORM_INT8;
    ImageInfo imgInfo = {};
    VAImageID imageId = 0;
    McsSurfaceInfo mcsSurfaceInfo = {};

    sharingFunctions->deriveImage(*surface, &vaImage);

    imageId = vaImage.image_id;

    imgDesc.image_width = vaImage.width;
    imgDesc.image_height = vaImage.height;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);

    if (plane == 0) {
        imgInfo.plane = GMM_PLANE_Y;
        channelOrder = CL_R;
    } else if (plane == 1) {
        imgInfo.plane = GMM_PLANE_U;
        channelOrder = CL_RG;
    } else {
        UNRECOVERABLE_IF(true);
    }

    auto gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport); //vaImage.format.fourcc == VA_FOURCC_NV12

    if (DebugManager.flags.EnableExtendedVaFormats.get() && vaImage.format.fourcc == VA_FOURCC_P010) {
        channelType = CL_UNORM_INT16;
        gmmSurfaceFormat = getExtendedSurfaceFormatInfo(vaImage.format.fourcc);
    }
    imgInfo.surfaceFormat = &gmmSurfaceFormat->surfaceFormat;

    cl_image_format imgFormat = {channelOrder, channelType};
    auto imgSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);

    sharingFunctions->extGetSurfaceHandle(surface, &sharedHandle);
    AllocationProperties properties(context->getDevice(0)->getRootDeviceIndex(), false, imgInfo, GraphicsAllocation::AllocationType::SHARED_IMAGE);

    auto alloc = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false);

    imgDesc.image_row_pitch = imgInfo.rowPitch;
    imgDesc.image_slice_pitch = 0u;
    imgInfo.slicePitch = 0u;
    imgInfo.surfaceFormat = &imgSurfaceFormat->surfaceFormat;
    if (plane == 1) {
        imgDesc.image_width /= 2;
        imgDesc.image_height /= 2;
        imgInfo.offset = vaImage.offsets[1];
        imgInfo.yOffset = 0;
        imgInfo.xOffset = 0;
        imgInfo.yOffsetForUVPlane = static_cast<uint32_t>(imgInfo.offset / vaImage.pitches[0]);
    }
    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);
    sharingFunctions->destroyImage(vaImage.image_id);

    auto vaSurface = new VASurface(sharingFunctions, imageId, plane, surface, context->getInteropUserSyncEnabled());

    auto image = Image::createSharedImage(context, vaSurface, mcsSurfaceInfo, alloc, nullptr, flags, imgSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
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

bool VASurface::validate(cl_mem_flags flags, cl_uint plane) {
    switch (flags) {
    case CL_MEM_READ_ONLY:
    case CL_MEM_WRITE_ONLY:
    case CL_MEM_READ_WRITE:
        break;
    default:
        return false;
    }
    if (plane > 1) {
        return false;
    }
    return true;
}

const ClSurfaceFormatInfo *VASurface::getExtendedSurfaceFormatInfo(uint32_t formatFourCC) {
    if (formatFourCC == VA_FOURCC_P010) {
        static const ClSurfaceFormatInfo formatInfo = {{CL_NV12_INTEL, CL_UNORM_INT16},
                                                       {GMM_RESOURCE_FORMAT::GMM_FORMAT_P010,
                                                        static_cast<GFX3DSTATE_SURFACEFORMAT>(NUM_GFX3DSTATE_SURFACEFORMATS), // not used for plane images
                                                        0,
                                                        1,
                                                        2,
                                                        2}};
        return &formatInfo;
    }
    return nullptr;
}

bool VASurface::isSupportedFourCC(int fourcc) {
    if ((fourcc == VA_FOURCC_NV12) ||
        (DebugManager.flags.EnableExtendedVaFormats.get() && fourcc == VA_FOURCC_P010)) {
        return true;
    }
    return false;
}
} // namespace NEO
