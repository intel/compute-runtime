/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unified_image.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"

namespace NEO {

Image *UnifiedImage::createSharedUnifiedImage(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description,
                                              const cl_image_format *imageFormat, const cl_image_desc *imageDesc, cl_int *errcodeRet) {

    auto *clSurfaceFormat = Image::getSurfaceFormatFromTable(flags, imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    ImageInfo imgInfo = {};
    imgInfo.imgDesc = Image::convertDescriptor(*imageDesc);
    imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;

    auto multiGraphicsAllocation = createMultiGraphicsAllocation(context, description, &imgInfo, AllocationType::sharedImage, errcodeRet);
    if (!multiGraphicsAllocation) {
        return nullptr;
    }

    for (auto graphicsAllocation : multiGraphicsAllocation->getGraphicsAllocations()) {
        swapGmm(graphicsAllocation, context, &imgInfo);

        auto &memoryManager = *context->getMemoryManager();
        if (graphicsAllocation->getDefaultGmm()->unifiedAuxTranslationCapable()) {
            const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
            const auto &productHelper = context->getDevice(0)->getProductHelper();
            graphicsAllocation->getDefaultGmm()->setCompressionEnabled(productHelper.isPageTableManagerSupported(hwInfo) ? memoryManager.mapAuxGpuVA(graphicsAllocation) : true);
        }
    }

    auto sharingHandler = new UnifiedImage(context->getSharing<UnifiedSharingFunctions>(), description.type);

    return Image::createSharedImage(context, sharingHandler, McsSurfaceInfo{}, std::move(*multiGraphicsAllocation), nullptr,
                                    flags, 0, clSurfaceFormat, imgInfo, __GMM_NO_CUBE_MAP, 0u, imageDesc->num_mip_levels, false);
}

} // namespace NEO
