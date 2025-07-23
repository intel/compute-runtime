/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/va/va_surface.h"

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/i915.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"

#include "drm_fourcc.h"

#include <va/va_drmcommon.h>

namespace NEO {

bool VASurface::isSupportedPlanarFormat(uint32_t imageFourcc) {
    switch (imageFourcc) {
    case VA_FOURCC_P010:
    case VA_FOURCC_P016:
    case VA_FOURCC_RGBP:
    case VA_FOURCC_NV12:
        return true;
    default:
        return false;
    }
}

bool VASurface::isSupportedPackedFormat(uint32_t imageFourcc) {
    switch (imageFourcc) {
    case VA_FOURCC_YUY2:
        return true;
    case VA_FOURCC_Y210:
        return true;
    case VA_FOURCC_ARGB:
        return true;
    default:
        return false;
    }
}

VAStatus VASurface::getSurfaceDescription(SharedSurfaceInfo &surfaceInfo, VASharingFunctions *sharingFunctions, VASurfaceID *surface) {
    VADRMPRIMESurfaceDescriptor vaDrmPrimeSurfaceDesc = {};
    auto vaStatus = sharingFunctions->exportSurfaceHandle(*surface,
                                                          VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                                          VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                                          &vaDrmPrimeSurfaceDesc);
    if (VA_STATUS_SUCCESS == vaStatus) {
        surfaceInfo.imageId = VA_INVALID_ID;
        surfaceInfo.imgInfo.imgDesc.imageWidth = vaDrmPrimeSurfaceDesc.width;
        surfaceInfo.imgInfo.imgDesc.imageHeight = vaDrmPrimeSurfaceDesc.height;
        surfaceInfo.imageFourcc = vaDrmPrimeSurfaceDesc.fourcc;
        surfaceInfo.imgInfo.linearStorage = DRM_FORMAT_MOD_LINEAR == vaDrmPrimeSurfaceDesc.objects[0].drm_format_modifier;
        surfaceInfo.sharedHandle = vaDrmPrimeSurfaceDesc.objects[0].fd;

        if (surfaceInfo.plane != 0 && !VASurface::isSupportedPlanarFormat(surfaceInfo.imageFourcc)) {
            return VA_STATUS_ERROR_INVALID_PARAMETER;
        }

        surfaceInfo.imagePitch = vaDrmPrimeSurfaceDesc.layers[0].pitch[0];
        surfaceInfo.imageOffset = vaDrmPrimeSurfaceDesc.layers[0].offset[0];

        if (surfaceInfo.plane == 1) {
            surfaceInfo.imageOffset = vaDrmPrimeSurfaceDesc.layers[1].offset[0];
            surfaceInfo.imagePitch = vaDrmPrimeSurfaceDesc.layers[1].pitch[0];
        } else if (surfaceInfo.plane == 2) {
            surfaceInfo.imageOffset = vaDrmPrimeSurfaceDesc.layers[2].offset[0];
            surfaceInfo.imagePitch = vaDrmPrimeSurfaceDesc.layers[2].pitch[0];
        }
    } else {
        VAImage vaImage = {};
        vaStatus = sharingFunctions->deriveImage(*surface, &vaImage);

        if (vaStatus != VA_STATUS_SUCCESS) {
            return vaStatus;
        }

        surfaceInfo.imageId = vaImage.image_id;
        surfaceInfo.imgInfo.imgDesc.imageWidth = vaImage.width;
        surfaceInfo.imgInfo.imgDesc.imageHeight = vaImage.height;
        surfaceInfo.imageFourcc = vaImage.format.fourcc;
        surfaceInfo.imgInfo.linearStorage = false;

        if (surfaceInfo.plane != 0 && !VASurface::isSupportedPlanarFormat(surfaceInfo.imageFourcc)) {
            return VA_STATUS_ERROR_INVALID_PARAMETER;
        }

        if (surfaceInfo.plane == 1) {
            surfaceInfo.imageOffset = vaImage.offsets[1];
            surfaceInfo.imagePitch = vaImage.pitches[0];
        } else if (surfaceInfo.plane == 2) {
            surfaceInfo.imageOffset = vaImage.offsets[2];
            surfaceInfo.imagePitch = vaImage.pitches[0];
        }

        vaStatus = sharingFunctions->extGetSurfaceHandle(surface, &surfaceInfo.sharedHandle);
    }

    return vaStatus;
}

void VASurface::applyPlanarOptions(SharedSurfaceInfo &sharedSurfaceInfo, cl_uint plane, cl_mem_flags flags, bool supportOcl21) {
    bool isRGBPFormat = debugManager.flags.EnableExtendedVaFormats.get() && sharedSurfaceInfo.imageFourcc == VA_FOURCC_RGBP;

    if (plane == 0) {
        sharedSurfaceInfo.imgInfo.plane = GMM_PLANE_Y;
        sharedSurfaceInfo.channelOrder = CL_R;
    } else if (plane == 1) {
        sharedSurfaceInfo.imgInfo.plane = GMM_PLANE_U;
        sharedSurfaceInfo.channelOrder = isRGBPFormat ? CL_R : CL_RG;
    } else if (plane == 2) {
        UNRECOVERABLE_IF(!isRGBPFormat);
        sharedSurfaceInfo.imgInfo.plane = GMM_PLANE_V;
        sharedSurfaceInfo.channelOrder = CL_R;
    } else {
        UNRECOVERABLE_IF(true);
    }

    auto gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &sharedSurfaceInfo.gmmImgFormat, supportOcl21); // vaImage.format.fourcc == VA_FOURCC_NV12

    if (debugManager.flags.EnableExtendedVaFormats.get() && sharedSurfaceInfo.imageFourcc == VA_FOURCC_RGBP) {
        sharedSurfaceInfo.channelType = CL_UNORM_INT8;
        gmmSurfaceFormat = VASurface::getExtendedSurfaceFormatInfo(sharedSurfaceInfo.imageFourcc);
    } else if (sharedSurfaceInfo.imageFourcc == VA_FOURCC_P010 || sharedSurfaceInfo.imageFourcc == VA_FOURCC_P016) {
        sharedSurfaceInfo.channelType = CL_UNORM_INT16;
        gmmSurfaceFormat = VASurface::getExtendedSurfaceFormatInfo(sharedSurfaceInfo.imageFourcc);
    }
    sharedSurfaceInfo.imgInfo.surfaceFormat = &gmmSurfaceFormat->surfaceFormat;
}

void VASurface::applyPlaneSettings(SharedSurfaceInfo &sharedSurfaceInfo, cl_uint plane) {
    bool isRGBPFormat = debugManager.flags.EnableExtendedVaFormats.get() && sharedSurfaceInfo.imageFourcc == VA_FOURCC_RGBP;

    sharedSurfaceInfo.imgInfo.slicePitch = 0u;
    sharedSurfaceInfo.imgInfo.yOffset = 0;
    sharedSurfaceInfo.imgInfo.xOffset = 0;
    if (plane == 1) {
        if (!isRGBPFormat) {
            sharedSurfaceInfo.imgInfo.imgDesc.imageWidth /= 2;
            sharedSurfaceInfo.imgInfo.imgDesc.imageHeight /= 2;
        }
        sharedSurfaceInfo.imgInfo.offset = sharedSurfaceInfo.imageOffset;
        sharedSurfaceInfo.imgInfo.yOffsetForUVPlane = static_cast<uint32_t>(sharedSurfaceInfo.imageOffset / sharedSurfaceInfo.imagePitch);
    }
    if (isRGBPFormat && plane == 2) {
        sharedSurfaceInfo.imgInfo.offset = sharedSurfaceInfo.imageOffset;
    }
}

void VASurface::applyPackedOptions(SharedSurfaceInfo &sharedSurfaceInfo) {
    if (sharedSurfaceInfo.imageFourcc == VA_FOURCC_Y210) {
        sharedSurfaceInfo.channelType = CL_UNORM_INT16;
        sharedSurfaceInfo.channelOrder = CL_RGBA;
    } else if (sharedSurfaceInfo.imageFourcc == VA_FOURCC_ARGB) {
        sharedSurfaceInfo.channelType = CL_UNORM_INT8;
        sharedSurfaceInfo.channelOrder = CL_RGBA;
    } else {
        sharedSurfaceInfo.channelType = CL_UNORM_INT8;
        sharedSurfaceInfo.channelOrder = CL_YUYV_INTEL;
    }
    sharedSurfaceInfo.imgInfo.surfaceFormat = &VASurface::getExtendedSurfaceFormatInfo(sharedSurfaceInfo.imageFourcc)->surfaceFormat;
}

Image *VASurface::createSharedVaSurface(Context *context, VASharingFunctions *sharingFunctions,
                                        cl_mem_flags flags, cl_mem_flags_intel flagsIntel, VASurfaceID *surface,
                                        cl_uint plane, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    auto memoryManager = context->getMemoryManager();
    McsSurfaceInfo mcsSurfaceInfo = {};

    SharedSurfaceInfo sharedSurfaceInfo{};
    sharedSurfaceInfo.plane = plane;

    std::unique_lock<std::mutex> lock(sharingFunctions->mutex);

    auto result = getSurfaceDescription(sharedSurfaceInfo, sharingFunctions, surface);

    if (result != VA_STATUS_SUCCESS) {
        *errcodeRet = static_cast<cl_int>(result);
        return nullptr;
    }

    sharedSurfaceInfo.imgInfo.imgDesc.imageType = ImageType::image2D;

    bool supportOcl21 = context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features;

    if (VASurface::isSupportedPlanarFormat(sharedSurfaceInfo.imageFourcc)) {
        applyPlanarOptions(sharedSurfaceInfo, plane, flags, supportOcl21);
    } else if (VASurface::isSupportedPackedFormat(sharedSurfaceInfo.imageFourcc)) {
        applyPackedOptions(sharedSurfaceInfo);
    } else {
        *errcodeRet = VA_STATUS_ERROR_INVALID_PARAMETER;
        return nullptr;
    }

    AllocationProperties properties(context->getDevice(0)->getRootDeviceIndex(),
                                    false, // allocateMemory
                                    &sharedSurfaceInfo.imgInfo, AllocationType::sharedImage,
                                    context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex()));

    MemoryManager::OsHandleData osHandleData{sharedSurfaceInfo.sharedHandle};
    auto alloc = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);

    memoryManager->closeSharedHandle(alloc);

    if (VASurface::isSupportedPlanarFormat(sharedSurfaceInfo.imageFourcc)) {
        applyPlaneSettings(sharedSurfaceInfo, plane);
    }

    if (VA_INVALID_ID != sharedSurfaceInfo.imageId) {
        sharingFunctions->destroyImage(sharedSurfaceInfo.imageId);
    }

    lock.unlock();

    cl_image_format imgFormat = {sharedSurfaceInfo.channelOrder, sharedSurfaceInfo.channelType};
    auto imgSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &imgFormat, supportOcl21);
    sharedSurfaceInfo.imgInfo.surfaceFormat = &imgSurfaceFormat->surfaceFormat;
    sharedSurfaceInfo.imgInfo.imgDesc.imageRowPitch = sharedSurfaceInfo.imgInfo.rowPitch;

    auto vaSurface = new VASurface(sharingFunctions, sharedSurfaceInfo.imageId, plane, surface, context->getInteropUserSyncEnabled());
    auto multiGraphicsAllocation = MultiGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(alloc);

    auto image = Image::createSharedImage(context, vaSurface, mcsSurfaceInfo, std::move(multiGraphicsAllocation), nullptr, flags, flagsIntel, imgSurfaceFormat, sharedSurfaceInfo.imgInfo, __GMM_NO_CUBE_MAP, 0, 0, false);
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
    case CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL:
    case CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL:
        break;
    default:
        return false;
    }
    if (plane > 1 && !debugManager.flags.EnableExtendedVaFormats.get()) {
        return false;
    }
    return true;
}

const ClSurfaceFormatInfo *VASurface::getExtendedSurfaceFormatInfo(uint32_t formatFourCC) {
    if (formatFourCC == VA_FOURCC_P010) {
        static const ClSurfaceFormatInfo formatInfoP010 = {{CL_NV12_INTEL, CL_UNORM_INT16},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_P010,
                                                            static_cast<SurfaceFormat>(NUM_GFX3DSTATE_SURFACEFORMATS), // not used for plane images
                                                            0,
                                                            1,
                                                            2,
                                                            2}};
        return &formatInfoP010;
    }
    if (formatFourCC == VA_FOURCC_P016) {
        static const ClSurfaceFormatInfo formatInfoP016 = {{CL_NV12_INTEL, CL_UNORM_INT16},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_P016,
                                                            static_cast<SurfaceFormat>(NUM_GFX3DSTATE_SURFACEFORMATS), // not used for plane images
                                                            0,
                                                            1,
                                                            2,
                                                            2}};
        return &formatInfoP016;
    }
    if (formatFourCC == VA_FOURCC_RGBP) {
        static const ClSurfaceFormatInfo formatInfoRGBP = {{CL_NV12_INTEL, CL_UNORM_INT8},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_RGBP,
                                                            static_cast<SurfaceFormat>(GFX3DSTATE_SURFACEFORMAT_R8_UNORM), // not used for plane images
                                                            0,
                                                            1,
                                                            1,
                                                            1}};
        return &formatInfoRGBP;
    }
    if (formatFourCC == VA_FOURCC_YUY2) {
        static const ClSurfaceFormatInfo formatInfoYUY2 = {{CL_YUYV_INTEL, CL_UNORM_INT8},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_YUY2,
                                                            static_cast<SurfaceFormat>(GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL),
                                                            0,
                                                            2,
                                                            1,
                                                            2}};
        return &formatInfoYUY2;
    }
    if (formatFourCC == VA_FOURCC_Y210) {
        static const ClSurfaceFormatInfo formatInfoY210 = {{CL_RGBA, CL_UNORM_INT16},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_Y210,
                                                            static_cast<SurfaceFormat>(GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM),
                                                            0,
                                                            4,
                                                            2,
                                                            8}};
        return &formatInfoY210;
    }

    if (formatFourCC == VA_FOURCC_ARGB) {
        static const ClSurfaceFormatInfo formatInfoARGB = {{CL_RGBA, CL_UNORM_INT8},
                                                           {GMM_RESOURCE_FORMAT::GMM_FORMAT_R8G8B8A8_UNORM_TYPE,
                                                            static_cast<SurfaceFormat>(GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM),
                                                            0,
                                                            4,
                                                            1,
                                                            4}};
        return &formatInfoARGB;
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
    if (debugManager.flags.EnableExtendedVaFormats.get() && fourcc == VA_FOURCC_RGBP) {
        return true;
    }
    return false;
}

} // namespace NEO
