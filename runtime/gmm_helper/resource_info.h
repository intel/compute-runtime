/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_lib.h"

#include <functional>
#include <memory>

namespace NEO {
class GmmResourceInfo {
  public:
    static GmmResourceInfo *create(GMM_RESCREATE_PARAMS *resourceCreateParams);

    static GmmResourceInfo *create(GMM_RESOURCE_INFO *inputGmmResourceInfo);

    MOCKABLE_VIRTUAL ~GmmResourceInfo() = default;

    MOCKABLE_VIRTUAL size_t getSizeAllocation() { return static_cast<size_t>(resourceInfo->GetSize(GMM_TOTAL_SURF)); }

    MOCKABLE_VIRTUAL size_t getBaseWidth() { return static_cast<size_t>(resourceInfo->GetBaseWidth()); }

    MOCKABLE_VIRTUAL size_t getBaseHeight() { return static_cast<size_t>(resourceInfo->GetBaseHeight()); }

    MOCKABLE_VIRTUAL size_t getBaseDepth() { return static_cast<size_t>(resourceInfo->GetBaseDepth()); }

    MOCKABLE_VIRTUAL size_t getArraySize() { return static_cast<size_t>(resourceInfo->GetArraySize()); }

    MOCKABLE_VIRTUAL size_t getRenderPitch() { return static_cast<size_t>(resourceInfo->GetRenderPitch()); }

    MOCKABLE_VIRTUAL uint32_t getNumSamples() { return resourceInfo->GetNumSamples(); }

    MOCKABLE_VIRTUAL uint32_t getQPitch() { return resourceInfo->GetQPitch(); }

    MOCKABLE_VIRTUAL uint32_t getBitsPerPixel() { return resourceInfo->GetBitsPerPixel(); }

    MOCKABLE_VIRTUAL uint32_t getHAlign() { return resourceInfo->GetHAlign(); }

    MOCKABLE_VIRTUAL uint32_t getHAlignSurfaceState() { return resourceInfo->GetHAlignSurfaceState(); }

    MOCKABLE_VIRTUAL uint32_t getVAlignSurfaceState() { return resourceInfo->GetVAlignSurfaceState(); }

    MOCKABLE_VIRTUAL uint32_t getMaxLod() { return resourceInfo->GetMaxLod(); }

    MOCKABLE_VIRTUAL uint32_t getTileModeSurfaceState() { return resourceInfo->GetTileModeSurfaceState(); }

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

    MOCKABLE_VIRTUAL uint32_t getMipTailStartLodSurfaceState() { return resourceInfo->GetMipTailStartLodSurfaceState(); }

    MOCKABLE_VIRTUAL bool is64KBPageSuitable() const { return resourceInfo->Is64KBPageSuitable(); }

    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *peekHandle() const { return resourceInfo.get(); }

  protected:
    using UniquePtrType = std::unique_ptr<GMM_RESOURCE_INFO, std::function<void(GMM_RESOURCE_INFO *)>>;

    GmmResourceInfo() = default;

    GmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams);

    GmmResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo);

    void createResourceInfo(GMM_RESOURCE_INFO *resourceInfoPtr);

    UniquePtrType resourceInfo;
};
} // namespace NEO
