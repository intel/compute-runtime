/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unified_image.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
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
                                              const cl_image_format *imageFormat, const cl_image_desc *imageDesc, cl_int *errcodeRet,
                                              const RootDeviceIndicesContainer &targetRootDeviceIndices) {

    auto *clSurfaceFormat = Image::getSurfaceFormatFromTable(flags, imageFormat);
    ImageInfo imgInfo = {};
    imgInfo.imgDesc = Image::convertDescriptor(*imageDesc);
    imgInfo.surfaceFormat = &clSurfaceFormat->surfaceFormat;

    auto multiGraphicsAllocation = createMultiGraphicsAllocation(context, description, &imgInfo, AllocationType::sharedImage, errcodeRet,
                                                                 targetRootDeviceIndices);
    if (!multiGraphicsAllocation) {
        return nullptr;
    }

    auto &executionEnvironment = *context->getDevice(0)->getExecutionEnvironment();

    for (auto graphicsAllocation : multiGraphicsAllocation->getGraphicsAllocations()) {
        if (graphicsAllocation == nullptr) {
            continue;
        }

        swapGmm(graphicsAllocation, context, &imgInfo);

        auto &memoryManager = *context->getMemoryManager();
        if (graphicsAllocation->getDefaultGmm()->unifiedAuxTranslationCapable()) {
            auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[graphicsAllocation->getRootDeviceIndex()];
            const auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
            const auto &productHelper = rootDeviceEnvironment.getProductHelper();
            graphicsAllocation->getDefaultGmm()->setCompressionEnabled(productHelper.isPageTableManagerSupported(hwInfo) ? memoryManager.mapAuxGpuVA(graphicsAllocation) : true);
        }
    }

    auto sharingHandler = new UnifiedImage(context->getSharing<UnifiedSharingFunctions>(), description.type);

    return Image::createSharedImage(context, sharingHandler, McsSurfaceInfo{}, std::move(*multiGraphicsAllocation), nullptr,
                                    flags, 0, clSurfaceFormat, imgInfo, gmmNoCubeMap, 0u, imageDesc->num_mip_levels, false, errcodeRet);
}

} // namespace NEO
