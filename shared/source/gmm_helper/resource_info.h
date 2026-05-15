/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <functional>
#include <memory>

namespace NEO {
class GmmClientContext;
class GmmResourceInfo : NonCopyableAndNonMovableClass {
  public:
    static GmmResourceInfo *create(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams);

    static GmmResourceInfo *create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo);

    static GmmResourceInfo *create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo, bool openingHandle);

    MOCKABLE_VIRTUAL ~GmmResourceInfo();

    MOCKABLE_VIRTUAL size_t getSizeAllocation();

    MOCKABLE_VIRTUAL size_t getBaseWidth();

    MOCKABLE_VIRTUAL size_t getBaseHeight();

    MOCKABLE_VIRTUAL size_t getBaseDepth();

    MOCKABLE_VIRTUAL size_t getArraySize();

    MOCKABLE_VIRTUAL size_t getRenderPitch();

    MOCKABLE_VIRTUAL uint64_t getDriverProtectionBits(uint32_t overrideUsage, bool compressionDenied);

    MOCKABLE_VIRTUAL bool isResourceDenyCompressionEnabled();

    MOCKABLE_VIRTUAL uint32_t getNumSamples();

    MOCKABLE_VIRTUAL uint32_t getQPitch();

    MOCKABLE_VIRTUAL uint32_t getBitsPerPixel();

    MOCKABLE_VIRTUAL uint32_t getHAlign();

    MOCKABLE_VIRTUAL uint32_t getHAlignSurfaceState();

    MOCKABLE_VIRTUAL uint32_t getVAlignSurfaceState();

    MOCKABLE_VIRTUAL uint32_t getMaxLod();

    MOCKABLE_VIRTUAL uint32_t getTileModeSurfaceState();

    MOCKABLE_VIRTUAL GMM_RESOURCE_FORMAT getResourceFormat();

    MOCKABLE_VIRTUAL GMM_SURFACESTATE_FORMAT getResourceFormatSurfaceState();

    MOCKABLE_VIRTUAL GMM_RESOURCE_TYPE getResourceType();

    MOCKABLE_VIRTUAL GMM_RESOURCE_FLAG *getResourceFlags();

    MOCKABLE_VIRTUAL GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo);

    MOCKABLE_VIRTUAL uint8_t cpuBlt(GMM_RES_COPY_BLT *resCopyBlt);

    MOCKABLE_VIRTUAL void *getSystemMemPointer();

    MOCKABLE_VIRTUAL uint32_t getRenderAuxPitchTiles();

    MOCKABLE_VIRTUAL uint32_t getAuxQPitch();

    MOCKABLE_VIRTUAL uint64_t getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE auxType);

    MOCKABLE_VIRTUAL uint32_t getMipTailStartLODSurfaceState();

    MOCKABLE_VIRTUAL bool is64KBPageSuitable() const;

    MOCKABLE_VIRTUAL bool isDisplayable() const;

    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *peekGmmResourceInfo() const;

    MOCKABLE_VIRTUAL GMM_RESOURCE_USAGE_TYPE getCachePolicyUsage() const;

    MOCKABLE_VIRTUAL void *peekHandle() const { return handle; }

    MOCKABLE_VIRTUAL size_t peekHandleSize() const { return handleSize; }

    MOCKABLE_VIRTUAL void refreshHandle();

  protected:
    using UniquePtrType = std::unique_ptr<GMM_RESOURCE_INFO, std::function<void(GMM_RESOURCE_INFO *)>>;

    GmmResourceInfo() = default;

    GmmResourceInfo(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams);

    GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo);

    GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo, bool openingHandle);

    void createResourceInfo(GMM_RESOURCE_INFO *resourceInfoPtr);
    void decodeResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo);

    UniquePtrType resourceInfo;

    GmmClientContext *clientContext = nullptr;

    uint64_t gmmResourceHandle = 0;
    void *handle = nullptr;
    size_t handleSize = 0;
};

static_assert(NEO::NonCopyableAndNonMovable<GmmResourceInfo>);

} // namespace NEO
