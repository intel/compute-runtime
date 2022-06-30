/*
 * Copyright (C) 2018-2022 Intel Corporation
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

#include <drm/drm_fourcc.h>
#include <va/va_drmcommon.h>

namespace NEO {
Image *VASurface::createSharedVaSurface(Context *context, VASharingFunctions *sharingFunctions,
                                        cl_mem_flags flags, cl_mem_flags_intel flagsIntel, VASurfaceID *surface,
                                        cl_uint plane, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    auto memoryManager = context->getMemoryManager();
    unsigned int sharedHandle = 0;
    VADRMPRIMESurfaceDescriptor vaDrmPrimeSurfaceDesc = {};
    VAImage vaImage = {};
    cl_image_desc imgDesc = {};
    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    cl_channel_order channelOrder = CL_RG;
    cl_channel_type channelType = CL_UNORM_INT8;
    ImageInfo imgInfo = {};
    VAImageID imageId = 0;
    McsSurfaceInfo mcsSurfaceInfo = {};
    VAStatus vaStatus;

    uint32_t imageFourcc = 0;
    size_t imageOffset = 0;
    size_t imagePitch = 0;

    std::unique_lock<std::mutex> lock(sharingFunctions->mutex);

    vaStatus = sharingFunctions->exportSurfaceHandle(*surface,
                                                     VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                                     VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                                     &vaDrmPrimeSurfaceDesc);
    if (VA_STATUS_SUCCESS == vaStatus) {
        imageId = VA_INVALID_ID;
        imgDesc.image_width = vaDrmPrimeSurfaceDesc.width;
        imgDesc.image_height = vaDrmPrimeSurfaceDesc.height;
        imageFourcc = vaDrmPrimeSurfaceDesc.fourcc;
        if (plane == 1) {
            imageOffset = vaDrmPrimeSurfaceDesc.layers[1].offset[0];
            imagePitch = vaDrmPrimeSurfaceDesc.layers[1].pitch[0];
        } else if (plane == 2) {
            imageOffset = vaDrmPrimeSurfaceDesc.layers[2].offset[0];
            imagePitch = vaDrmPrimeSurfaceDesc.layers[2].pitch[0];
        }
        imgInfo.linearStorage = DRM_FORMAT_MOD_LINEAR == vaDrmPrimeSurfaceDesc.objects[0].drm_format_modifier;
        sharedHandle = vaDrmPrimeSurfaceDesc.objects[0].fd;
    } else {
        sharingFunctions->deriveImage(*surface, &vaImage);
        imageId = vaImage.image_id;
        imgDesc.image_width = vaImage.width;
        imgDesc.image_height = vaImage.height;
        imageFourcc = vaImage.format.fourcc;
        if (plane == 1) {
            imageOffset = vaImage.offsets[1];
            imagePitch = vaImage.pitches[0];
        } else if (plane == 2) {
            imageOffset = vaImage.offsets[2];
            imagePitch = vaImage.pitches[0];
        }
        imgInfo.linearStorage = false;
        sharingFunctions->extGetSurfaceHandle(surface, &sharedHandle);
    }

    bool isRGBPFormat = DebugManager.flags.EnableExtendedVaFormats.get() && imageFourcc == VA_FOURCC_RGBP;

    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);

    if (plane == 0) {
        imgInfo.plane = GMM_PLANE_Y;
        channelOrder = CL_R;
    } else if (plane == 1) {
        imgInfo.plane = GMM_PLANE_U;
        channelOrder = isRGBPFormat ? CL_R : CL_RG;
    } else if (plane == 2) {
        UNRECOVERABLE_IF(!isRGBPFormat);
        imgInfo.plane = GMM_PLANE_V;
        channelOrder = CL_R;
    } else {
        UNRECOVERABLE_IF(true);
    }

    auto gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features); //vaImage.format.fourcc == VA_FOURCC_NV12

    if (DebugManager.flags.EnableExtendedVaFormats.get() && imageFourcc == VA_FOURCC_RGBP) {
        channelType = CL_UNORM_INT8;
        gmmSurfaceFormat = getExtendedSurfaceFormatInfo(imageFourcc);
    } else if (imageFourcc == VA_FOURCC_P010 || imageFourcc == VA_FOURCC_P016) {
        channelType = CL_UNORM_INT16;
        gmmSurfaceFormat = getExtendedSurfaceFormatInfo(imageFourcc);
    }
    imgInfo.surfaceFormat = &gmmSurfaceFormat->surfaceFormat;

    cl_image_format imgFormat = {channelOrder, channelType};
    auto imgSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    AllocationProperties properties(context->getDevice(0)->getRootDeviceIndex(),
                                    false, // allocateMemory
                                    imgInfo, AllocationType::SHARED_IMAGE,
                                    context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex()));

    auto alloc = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false, false);

    memoryManager->closeSharedHandle(alloc);

    lock.unlock();

    imgDesc.image_row_pitch = imgInfo.rowPitch;
    imgDesc.image_slice_pitch = 0u;
    imgInfo.slicePitch = 0u;
    imgInfo.surfaceFormat = &imgSurfaceFormat->surfaceFormat;
    imgInfo.yOffset = 0;
    imgInfo.xOffset = 0;
    if (plane == 1) {
        if (!isRGBPFormat) {
            imgDesc.image_width /= 2;
            imgDesc.image_height /= 2;
        }
        imgInfo.offset = imageOffset;
        imgInfo.yOffsetForUVPlane = static_cast<uint32_t>(imageOffset / imagePitch);
    }
    if (isRGBPFormat && plane == 2) {
        imgInfo.offset = imageOffset;
    }
    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);
    if (VA_INVALID_ID != imageId) {
        sharingFunctions->destroyImage(imageId);
    }

    auto vaSurface = new VASurface(sharingFunctions, imageId, plane, surface, context->getInteropUserSyncEnabled());
    auto multiGraphicsAllocation = MultiGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(alloc);

    auto image = Image::createSharedImage(context, vaSurface, mcsSurfaceInfo, std::move(multiGraphicsAllocation), nullptr, flags, flagsIntel, imgSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0, 0);
    image->setMediaPlaneType(plane);
    return image;
}

void VASurface::synchronizeObject(UpdateData &updateData) {
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
    if (!interopUserSync) {
        if (sharingFunctions->syncSurface(surfaceId) != VA_STATUS_SUCCESS) {
            updateData.synchronizationStatus = SYNCHRONIZE_ERROR;
        }
    }
}

void VASurface::getMemObjectInfo(size_t &paramValueSize, void *&paramValue) {
    paramValueSize = sizeof(surfaceIdPtr);
    paramValue = &surfaceIdPtr;
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
    if (plane > 1 && !DebugManager.flags.EnableExtendedVaFormats.get()) {
        return false;
    }
    return true;
}

const ClSurfaceFormatInfo *VASurface::getExtendedSurfaceFormatInfo(uint32_t formatFourCC) {
    if (formatFourCC == VA_FOURCC_P010) {
        static const ClSurfaceFormatInfo formatInfoP010 = {{CL_NV12_INTEL, CL_UNORM_INT16},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_P010,
                                                            static_cast<GFX3DSTATE_SURFACEFORMAT>(NUM_GFX3DSTATE_SURFACEFORMATS), // not used for plane images
                                                            0,
                                                            1,
                                                            2,
                                                            2}};
        return &formatInfoP010;
    }
    if (formatFourCC == VA_FOURCC_P016) {
        static const ClSurfaceFormatInfo formatInfoP016 = {{CL_NV12_INTEL, CL_UNORM_INT16},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_P016,
                                                            static_cast<GFX3DSTATE_SURFACEFORMAT>(NUM_GFX3DSTATE_SURFACEFORMATS), // not used for plane images
                                                            0,
                                                            1,
                                                            2,
                                                            2}};
        return &formatInfoP016;
    }
    if (formatFourCC == VA_FOURCC_RGBP) {
        static const ClSurfaceFormatInfo formatInfoRGBP = {{CL_NV12_INTEL, CL_UNORM_INT8},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_RGBP,
                                                            static_cast<GFX3DSTATE_SURFACEFORMAT>(GFX3DSTATE_SURFACEFORMAT_R8_UNORM), // not used for plane images
                                                            0,
                                                            1,
                                                            1,
                                                            1}};
        return &formatInfoRGBP;
    }
    return nullptr;
}

bool VASurface::isSupportedFourCCTwoPlaneFormat(int fourcc) {
    if ((fourcc == VA_FOURCC_NV12) ||
        (fourcc == VA_FOURCC_P010) ||
        (fourcc == VA_FOURCC_P016)) {
        return true;
    }
    return false;
}

bool VASurface::isSupportedFourCCThreePlaneFormat(int fourcc) {
    if (DebugManager.flags.EnableExtendedVaFormats.get() && fourcc == VA_FOURCC_RGBP) {
        return true;
    }
    return false;
}

} // namespace NEO
