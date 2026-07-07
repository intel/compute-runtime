/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/va/leo_va_surface.h"

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/i915.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"

#include "drm_fourcc.h"

#include <va/va_drmcommon.h>

namespace NEO {
namespace LEO {

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

void VASurface::applyPlanarOptions(SharedSurfaceInfo &sharedSurfaceInfo, cl_uint plane, cl_mem_flags flags) {
    bool isRGBPFormat = debugManager.flags.EnableExtendedVaFormats.get() && sharedSurfaceInfo.imageFourcc == VA_FOURCC_RGBP;

    if (plane == 0) {
        sharedSurfaceInfo.imgInfo.plane = ImagePlane::planeY;
        sharedSurfaceInfo.channelOrder = CL_R;
    } else if (plane == 1) {
        sharedSurfaceInfo.imgInfo.plane = ImagePlane::planeU;
        sharedSurfaceInfo.channelOrder = isRGBPFormat ? CL_R : CL_RG;
    } else if (plane == 2) {
        UNRECOVERABLE_IF(!isRGBPFormat);
        sharedSurfaceInfo.imgInfo.plane = ImagePlane::planeV;
        sharedSurfaceInfo.channelOrder = CL_R;
    } else {
        UNRECOVERABLE_IF(true);
    }

    auto gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &sharedSurfaceInfo.gmmImgFormat); // vaImage.format.fourcc == VA_FOURCC_NV12

    if (debugManager.flags.EnableExtendedVaFormats.get() && sharedSurfaceInfo.imageFourcc == VA_FOURCC_RGBP) {
        sharedSurfaceInfo.channelType = CL_UNORM_INT8;
        gmmSurfaceFormat = VASurface::getExtendedSurfaceFormatInfo(sharedSurfaceInfo.imageFourcc);
    } else if (sharedSurfaceInfo.imageFourcc == VA_FOURCC_P010 || sharedSurfaceInfo.imageFourcc == VA_FOURCC_P016) {
        sharedSurfaceInfo.channelType = CL_UNORM_INT16;
        gmmSurfaceFormat = VASurface::getExtendedSurfaceFormatInfo(sharedSurfaceInfo.imageFourcc);
    }
    sharedSurfaceInfo.imgInfo.surfaceFormat = &gmmSurfaceFormat->surfaceFormat;
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
}

Image *VASurface::createSharedVaSurface(Context *context, VASharingFunctions *sharingFunctions,
                                        cl_mem_flags flags, cl_mem_flags_intel flagsIntel, VASurfaceID *surface,
                                        cl_uint plane, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    SharedSurfaceInfo sharedSurfaceInfo{};
    sharedSurfaceInfo.plane = plane;

    std::unique_lock<std::mutex> lock(sharingFunctions->mutex);

    auto result = getSurfaceDescription(sharedSurfaceInfo, sharingFunctions, surface);

    if (result != VA_STATUS_SUCCESS) {
        errorCode.set(static_cast<cl_int>(result));
        return nullptr;
    }

    if (VA_INVALID_ID != sharedSurfaceInfo.imageId) {
        sharingFunctions->destroyImage(sharedSurfaceInfo.imageId);
    }

    lock.unlock();

    ze_external_memory_import_fd_t l0imageHandleDesc{ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD};
    l0imageHandleDesc.fd = sharedSurfaceInfo.sharedHandle;
    l0imageHandleDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;

    ze_image_desc_t l0imageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};
    l0imageDesc.miplevels = 0;
    l0imageDesc.pNext = &l0imageHandleDesc;
    l0imageDesc.type = ze_image_type_t::ZE_IMAGE_TYPE_2D;
    l0imageDesc.width = static_cast<uint32_t>(sharedSurfaceInfo.imgInfo.imgDesc.imageWidth);
    l0imageDesc.height = static_cast<uint32_t>(sharedSurfaceInfo.imgInfo.imgDesc.imageHeight);
    l0imageDesc.depth = 1;
    l0imageDesc.arraylevels = 0;

    bool needsView = true;

    switch (sharedSurfaceInfo.imageFourcc) {
    case VA_FOURCC_P010:
        l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_P010;
        break;
    case VA_FOURCC_P016:
        l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_P016;
        break;
    case VA_FOURCC_RGBP:
        l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_RGBP;
        break;
    case VA_FOURCC_NV12:
        l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_NV12;
        break;
    case VA_FOURCC_YUY2:
        l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_YUY2;
        break;
    case VA_FOURCC_Y210:
        l0imageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_Y210;
        break;
    default:
        needsView = false;
        break;
    }

    ze_result_t ret = ZE_RESULT_SUCCESS;
    ze_image_handle_t baseImageHandle{};
    ze_image_view_planar_ext_desc_t l0imagePlannarDesc{ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXT_DESC};

    if (needsView) {
        ret = zeImageCreate(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, &baseImageHandle);

        if (ret != ZE_RESULT_SUCCESS) {
            errorCode.set(L0ToClResultMapper(ret));
            return nullptr;
        }

        l0imagePlannarDesc.planeIndex = plane;
        l0imageDesc.pNext = &l0imagePlannarDesc;
    }

    if (VASurface::isSupportedPlanarFormat(sharedSurfaceInfo.imageFourcc)) {
        applyPlanarOptions(sharedSurfaceInfo, plane, flags);
    } else if (VASurface::isSupportedPackedFormat(sharedSurfaceInfo.imageFourcc)) {
        applyPackedOptions(sharedSurfaceInfo);
    } else {
        errorCode.set(VA_STATUS_ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    cl_image_format imgFormat = {sharedSurfaceInfo.channelOrder, sharedSurfaceInfo.channelType};
    Image::clToL0ImageFormat(l0imageDesc.format, imgFormat.image_channel_order, imgFormat.image_channel_data_type);

    ze_srgb_ext_desc_t srgbExtDesc{ZE_STRUCTURE_TYPE_SRGB_EXT_DESC, l0imageDesc.pNext, Image::isSRGB(imgFormat.image_channel_order)};
    l0imageDesc.pNext = &srgbExtDesc;

    ze_image_handle_t imageHandle{};

    if (needsView) {
        ret = zeImageViewCreateExp(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, baseImageHandle, &imageHandle);
    } else {
        ret = zeImageCreate(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, &imageHandle);
    }

    if (ret != ZE_RESULT_SUCCESS) {
        errorCode.set(L0ToClResultMapper(ret));
        return nullptr;
    }

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto image = new Image(context, memoryProperties, flags, imageHandle, nullptr, baseImageHandle, false, imgFormat, nullptr);

    auto vaSurface = new VASurface(sharingFunctions, sharedSurfaceInfo.imageId, plane, surface, context->getInteropUserSyncEnabled());
    image->setSharingHandler(vaSurface);

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

} // namespace LEO
} // namespace NEO
