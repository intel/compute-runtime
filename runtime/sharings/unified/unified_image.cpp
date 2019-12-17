/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unified_image.h"

#include "core/helpers/hw_helper.h"
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/context/context.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/get_info.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {

Image *UnifiedImage::createSharedUnifiedImage(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description,
                                              const cl_image_format *imageFormat, const cl_image_desc *imageDesc, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);
    UnifiedSharingFunctions *sharingFunctions = context->getSharing<UnifiedSharingFunctions>();

    ImageInfo imgInfo = {};
    imgInfo.imgDesc = imageDesc;
    imgInfo.surfaceFormat = Image::getSurfaceFormatFromTable(flags, imageFormat);

    GraphicsAllocation *graphicsAllocation = createGraphicsAllocation(context, description);
    if (!graphicsAllocation) {
        errorCode.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    graphicsAllocation->getDefaultGmm()->updateOffsetsInImgInfo(imgInfo, 0u);

    auto &memoryManager = *context->getMemoryManager();
    if (graphicsAllocation->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        const auto &hwInfo = *memoryManager.peekExecutionEnvironment().getHardwareInfo();
        const auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        graphicsAllocation->getDefaultGmm()->isRenderCompressed = hwHelper.isPageTableManagerSupported(hwInfo) ? memoryManager.mapAuxGpuVA(graphicsAllocation) : true;
    }

    const uint32_t baseMipmapIndex = 0u;
    const uint32_t sharedMipmapsCount = imageDesc->num_mip_levels;
    auto sharingHandler = new UnifiedImage(sharingFunctions, description.type);
    return Image::createSharedImage(context, sharingHandler, McsSurfaceInfo{}, graphicsAllocation, nullptr,
                                    flags, imgInfo, __GMM_NO_CUBE_MAP, baseMipmapIndex, sharedMipmapsCount);
}

GraphicsAllocation *UnifiedImage::createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description) {
    switch (description.type) {
    case UnifiedSharingHandleType::Win32Nt: {
        auto graphicsAllocation = context->getMemoryManager()->createGraphicsAllocationFromNTHandle(description.handle, 0u);
        return graphicsAllocation;
    }
    default:
        return nullptr;
    }
}

} // namespace NEO
