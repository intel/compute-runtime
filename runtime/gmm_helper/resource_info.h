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

#pragma once
#include "runtime/gmm_helper/gmm_lib.h"
#include <memory>
#include <functional>

namespace OCLRT {
class GmmResourceInfo {
  public:
    static GmmResourceInfo *create(GMM_RESCREATE_PARAMS *resourceCreateParams);

    static GmmResourceInfo *create(GMM_RESOURCE_INFO *inputGmmResourceInfo);

    MOCKABLE_VIRTUAL ~GmmResourceInfo() = default;

    MOCKABLE_VIRTUAL size_t getSizeAllocation() { return static_cast<size_t>(resourceInfo->GetSizeAllocation()); }

    MOCKABLE_VIRTUAL size_t getBaseWidth() { return static_cast<size_t>(resourceInfo->GetBaseWidth()); }

    MOCKABLE_VIRTUAL size_t getBaseHeight() { return static_cast<size_t>(resourceInfo->GetBaseHeight()); }

    MOCKABLE_VIRTUAL size_t getBaseDepth() { return static_cast<size_t>(resourceInfo->GetBaseDepth()); }

    MOCKABLE_VIRTUAL size_t getArraySize() { return static_cast<size_t>(resourceInfo->GetArraySize()); }

    MOCKABLE_VIRTUAL size_t getRenderPitch() { return static_cast<size_t>(resourceInfo->GetRenderPitch()); }

    MOCKABLE_VIRTUAL uint32_t getNumSamples() { return resourceInfo->GetNumSamples(); }

    MOCKABLE_VIRTUAL uint32_t getQPitch() { return resourceInfo->GetQPitch(); }

    MOCKABLE_VIRTUAL uint32_t getBitsPerPixel() { return resourceInfo->GetBitsPerPixel(); }

    MOCKABLE_VIRTUAL uint32_t getHAlign() { return resourceInfo->GetHAlign(); }

    MOCKABLE_VIRTUAL uint32_t getVAlign() { return resourceInfo->GetVAlign(); }

    MOCKABLE_VIRTUAL uint32_t getMaxLod() { return resourceInfo->GetMaxLod(); }

    MOCKABLE_VIRTUAL GMM_TILE_TYPE getTileType() { return resourceInfo->GetTileType(); }

    MOCKABLE_VIRTUAL GMM_RESOURCE_FORMAT getResourceFormat() { return resourceInfo->GetResourceFormat(); }

    MOCKABLE_VIRTUAL GMM_SURFACESTATE_FORMAT getResourceFormatSurfaceState() { return resourceInfo->GetResourceFormatSurfaceState(); }

    MOCKABLE_VIRTUAL GMM_RESOURCE_TYPE getResourceType() { return resourceInfo->GetResourceType(); }

    MOCKABLE_VIRTUAL GMM_RESOURCE_FLAG *getResourceFlags() { return &resourceInfo->GetResFlags(); }

    MOCKABLE_VIRTUAL GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) { return resourceInfo->GetOffset(reqOffsetInfo); }

    MOCKABLE_VIRTUAL uint8_t cpuBlt(GMM_RES_COPY_BLT *resCopyBlt) { return resourceInfo->CpuBlt(resCopyBlt); }

    MOCKABLE_VIRTUAL void *getSystemMemPointer(uint8_t isD3DDdiAllocation) { return resourceInfo->GetSystemMemPointer(isD3DDdiAllocation); }

    MOCKABLE_VIRTUAL uint32_t getRenderAuxPitchTiles() { return resourceInfo->GetRenderAuxPitchTiles(); }

    MOCKABLE_VIRTUAL uint32_t getAuxQPitch() { return resourceInfo->GetAuxQPitch(); }

    MOCKABLE_VIRTUAL uint64_t getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE auxType) { return resourceInfo->GetUnifiedAuxSurfaceOffset(auxType); }

    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *peekHandle() const { return resourceInfo.get(); }

  protected:
    static void customDeleter(GMM_RESOURCE_INFO *gmmResourceInfoHandle);
    using UniquePtrType = std::unique_ptr<GMM_RESOURCE_INFO, std::function<void(GMM_RESOURCE_INFO *)>>;

    GmmResourceInfo() = default;

    GmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams);

    GmmResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo);

    UniquePtrType resourceInfo;
};
} // namespace OCLRT
