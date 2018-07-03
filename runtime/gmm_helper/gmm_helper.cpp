/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/sku_info/operations/sku_info_transfer.h"
#include "runtime/os_interface/os_library.h"

namespace OCLRT {
void GmmHelper::initContext(const PLATFORM *pPlatform,
                            const FeatureTable *pSkuTable,
                            const WorkaroundTable *pWaTable,
                            const GT_SYSTEM_INFO *pGtSysInfo) {
    if (!GmmHelper::gmmClientContext) {
        _SKU_FEATURE_TABLE gmmFtrTable = {};
        _WA_TABLE gmmWaTable = {};
        SkuInfoTransfer::transferFtrTableForGmm(&gmmFtrTable, pSkuTable);
        SkuInfoTransfer::transferWaTableForGmm(&gmmWaTable, pWaTable);
        loadLib();
        bool success = GMM_SUCCESS == initGlobalContextFunc(*pPlatform, &gmmFtrTable, &gmmWaTable, pGtSysInfo, GMM_CLIENT::GMM_OCL_VISTA);
        UNRECOVERABLE_IF(!success);
        GmmHelper::gmmClientContext = GmmHelper::createClientContextFunc(GMM_CLIENT::GMM_OCL_VISTA);
    }
    UNRECOVERABLE_IF(!GmmHelper::gmmClientContext);
}

void GmmHelper::destroyContext() {
    if (GmmHelper::gmmClientContext) {
        deleteClientContextFunc(GmmHelper::gmmClientContext);
        GmmHelper::gmmClientContext = nullptr;
        destroyGlobalContextFunc();
        if (gmmLib) {
            delete gmmLib;
            gmmLib = nullptr;
        }
    }
}

uint32_t GmmHelper::getMOCS(uint32_t type) {
    if (useSimplifiedMocsTable) {
        if (type == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) {
            return cacheDisabledIndex;
        } else {
            return cacheEnabledIndex;
        }
    }
    MEMORY_OBJECT_CONTROL_STATE mocs = GmmHelper::gmmClientContext->CachePolicyGetMemoryObject(nullptr, static_cast<GMM_RESOURCE_USAGE_TYPE>(type));

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

uint32_t GmmHelper::getRenderTileMode(uint32_t tileWalk) {
    uint32_t tileMode = 0; // TILE_MODE_LINEAR
    if (tileWalk == 0) {
        tileMode = 2; // TILE_MODE_XMAJOR;
    } else if (tileWalk == 1) {
        tileMode = 3; // TILE_MODE_YMAJOR;
    } else if (tileWalk == 2) {
        tileMode = 1; // TILE_MODE_WMAJOR;
    }
    return tileMode;
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
GmmHelper::GmmHelper(const HardwareInfo *pHwInfo) {
    GmmHelper::hwInfo = pHwInfo;
    initContext(pHwInfo->pPlatform, pHwInfo->pSkuTable, pHwInfo->pWaTable, pHwInfo->pSysInfo);
}
GmmHelper::~GmmHelper() {
    if (isLoaded) {
        destroyContext();
    }
}
bool GmmHelper::useSimplifiedMocsTable = false;
GMM_CLIENT_CONTEXT *GmmHelper::gmmClientContext = nullptr;
const HardwareInfo *GmmHelper::hwInfo = nullptr;
OsLibrary *GmmHelper::gmmLib = nullptr;
} // namespace OCLRT
