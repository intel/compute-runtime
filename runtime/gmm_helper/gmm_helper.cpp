/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"

#include "core/helpers/aligned_memory.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/platform/platform.h"
#include "runtime/sku_info/operations/sku_info_transfer.h"

#include "gmm_client_context.h"

namespace NEO {

GmmClientContext *GmmHelper::getClientContext() {
    return getInstance()->gmmClientContext.get();
}

const HardwareInfo *GmmHelper::getHardwareInfo() {
    return hwInfo;
}

GmmHelper *GmmHelper::getInstance() {
    return platform()->peekExecutionEnvironment()->getGmmHelper();
}

void GmmHelper::setSimplifiedMocsTableUsage(bool value) {
    useSimplifiedMocsTable = value;
}

void GmmHelper::initContext(const PLATFORM *platform,
                            const FeatureTable *featureTable,
                            const WorkaroundTable *workaroundTable,
                            const GT_SYSTEM_INFO *pGtSysInfo) {
    _SKU_FEATURE_TABLE gmmFtrTable = {};
    _WA_TABLE gmmWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&gmmFtrTable, featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&gmmWaTable, workaroundTable);
    loadLib();
    bool success = GMM_SUCCESS == gmmEntries.pfnCreateSingletonContext(*platform, &gmmFtrTable, &gmmWaTable, pGtSysInfo);
    UNRECOVERABLE_IF(!success);
    gmmClientContext = GmmHelper::createGmmContextWrapperFunc(GMM_CLIENT::GMM_OCL_VISTA, gmmEntries);
    UNRECOVERABLE_IF(!gmmClientContext);
}

uint32_t GmmHelper::getMOCS(uint32_t type) {
    if (useSimplifiedMocsTable) {
        if (type == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) {
            return cacheDisabledIndex;
        } else {
            return cacheEnabledIndex;
        }
    }
    MEMORY_OBJECT_CONTROL_STATE mocs = gmmClientContext->cachePolicyGetMemoryObject(nullptr, static_cast<GMM_RESOURCE_USAGE_TYPE>(type));

    return static_cast<uint32_t>(mocs.DwordValue);
}

void GmmHelper::queryImgFromBufferParams(ImageInfo &imgInfo, GraphicsAllocation *gfxAlloc) {
    // 1D or 2D from buffer
    if (imgInfo.imgDesc->image_row_pitch > 0) {
        imgInfo.rowPitch = imgInfo.imgDesc->image_row_pitch;
    } else {
        imgInfo.rowPitch = getValidParam(imgInfo.imgDesc->image_width) * imgInfo.surfaceFormat->ImageElementSizeInBytes;
    }
    imgInfo.slicePitch = imgInfo.rowPitch * getValidParam(imgInfo.imgDesc->image_height);
    imgInfo.size = gfxAlloc->getUnderlyingBufferSize();
    imgInfo.qPitch = 0;
}

bool GmmHelper::allowTiling(const cl_image_desc &imageDesc) {
    // consider returning tiling type instead of bool
    if (DebugManager.flags.ForceLinearImages.get()) {
        return false;
    } else {
        auto imageType = imageDesc.image_type;

        if (imageType == CL_MEM_OBJECT_IMAGE1D ||
            imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
            imageType == CL_MEM_OBJECT_IMAGE1D_BUFFER ||
            ((imageType == CL_MEM_OBJECT_IMAGE2D && imageDesc.buffer))) {
            return false;
        } else {
            return true;
        }
    }
}

uint64_t GmmHelper::canonize(uint64_t address) {
    return ((int64_t)((address & 0xFFFFFFFFFFFF) << (64 - 48))) >> (64 - 48);
}

uint64_t GmmHelper::decanonize(uint64_t address) {
    return (uint64_t)(address & 0xFFFFFFFFFFFF);
}

uint32_t GmmHelper::getRenderAlignment(uint32_t alignment) {
    uint32_t returnAlign = 0;
    if (alignment == 8) {
        returnAlign = 2;
    } else if (alignment == 16) {
        returnAlign = 3;
    } else {
        returnAlign = 1;
    }
    return returnAlign;
}

uint32_t GmmHelper::getRenderMultisamplesCount(uint32_t numSamples) {
    if (numSamples == 2) {
        return 1;
    } else if (numSamples == 4) {
        return 2;
    } else if (numSamples == 8) {
        return 3;
    } else if (numSamples == 16) {
        return 4;
    }
    return 0;
}

GMM_YUV_PLANE GmmHelper::convertPlane(OCLPlane oclPlane) {
    if (oclPlane == OCLPlane::PLANE_Y) {
        return GMM_PLANE_Y;
    } else if (oclPlane == OCLPlane::PLANE_U || oclPlane == OCLPlane::PLANE_UV) {
        return GMM_PLANE_U;
    } else if (oclPlane == OCLPlane::PLANE_V) {
        return GMM_PLANE_V;
    }

    return GMM_NO_PLANE;
}
GmmHelper::GmmHelper(const HardwareInfo *pHwInfo) : hwInfo(pHwInfo) {
    initContext(&pHwInfo->platform, &pHwInfo->featureTable, &pHwInfo->workaroundTable, &pHwInfo->gtSystemInfo);
}
GmmHelper::~GmmHelper() {
    gmmEntries.pfnDestroySingletonContext();
};
decltype(GmmHelper::createGmmContextWrapperFunc) GmmHelper::createGmmContextWrapperFunc = GmmClientContextBase::create<GmmClientContext>;
} // namespace NEO
