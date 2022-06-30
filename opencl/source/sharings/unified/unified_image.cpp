/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unified_image.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"

namespace NEO {

Image *UnifiedImage::createSharedUnifiedImage(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description,
                                              const cl_image_format *imageFormat, const cl_image_desc *imageDesc, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);
    UnifiedSharingFunctions *sharingFunctions = context->getSharing<UnifiedSharingFunctions>();

    auto *clSurfaceFormat = Image::getSurfaceFormatFromTable(flags, imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    ImageInfo imgInfo = {};
    imgInfo.imgDesc = Image::convertDescriptor(*imageDesc);
    imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;

    GraphicsAllocation *graphicsAllocation = createGraphicsAllocation(context, description, AllocationType::SHARED_IMAGE);
    if (!graphicsAllocation) {
        errorCode.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    graphicsAllocation->getDefaultGmm()->updateOffsetsInImgInfo(imgInfo, 0u);

    auto &memoryManager = *context->getMemoryManager();
    if (graphicsAllocation->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
        const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
        graphicsAllocation->getDefaultGmm()->isCompressionEnabled = hwInfoConfig.isPageTableManagerSupported(hwInfo) ? memoryManager.mapAuxGpuVA(graphicsAllocation) : true;
    }

    const uint32_t baseMipmapIndex = 0u;
    const uint32_t sharedMipmapsCount = imageDesc->num_mip_levels;
    auto sharingHandler = new UnifiedImage(sharingFunctions, description.type);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(graphicsAllocation);

    return Image::createSharedImage(context, sharingHandler, McsSurfaceInfo{}, std::move(multiGraphicsAllocation), nullptr,
                                    flags, 0, clSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, baseMipmapIndex, sharedMipmapsCount);
}

} // namespace NEO
