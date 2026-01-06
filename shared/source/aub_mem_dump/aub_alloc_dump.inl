/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"

#include "aubstream/aubstream.h"

using namespace NEO;
using namespace aub_stream;

namespace AubAllocDump {

template <typename GfxFamily>
uint32_t getImageSurfaceTypeFromGmmResourceType(GMM_RESOURCE_TYPE gmmResourceType) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    auto surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL;

    switch (gmmResourceType) {
    case GMM_RESOURCE_TYPE::RESOURCE_1D:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
        break;
    case GMM_RESOURCE_TYPE::RESOURCE_2D:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D;
        break;
    case GMM_RESOURCE_TYPE::RESOURCE_3D:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D;
        break;
    default:
        DEBUG_BREAK_IF(true);
        break;
    }
    return surfaceType;
}

template <typename GfxFamily>
SurfaceInfo *getDumpSurfaceInfo(GraphicsAllocation &gfxAllocation, const GmmHelper &gmmHelper, DumpFormat dumpFormat) {
    SurfaceInfo *surfaceInfo = nullptr;

    if (isBufferDumpFormat(dumpFormat)) {
        using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
        using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
        surfaceInfo = new SurfaceInfo();
        surfaceInfo->address = gmmHelper.decanonize(gfxAllocation.getGpuAddress());
        surfaceInfo->width = static_cast<uint32_t>(gfxAllocation.getUnderlyingBufferSize());
        surfaceInfo->height = 1;
        surfaceInfo->pitch = static_cast<uint32_t>(gfxAllocation.getUnderlyingBufferSize());
        surfaceInfo->format = SURFACE_FORMAT::SURFACE_FORMAT_RAW;
        surfaceInfo->tilingType = RENDER_SURFACE_STATE::TILE_MODE_LINEAR;
        surfaceInfo->surftype = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
        surfaceInfo->compressed = gfxAllocation.isCompressionEnabled();
        surfaceInfo->dumpType = (AubAllocDump::DumpFormat::BUFFER_TRE == dumpFormat) ? dumpType::tre : dumpType::bin;
    } else if (isImageDumpFormat(dumpFormat)) {
        auto gmm = gfxAllocation.getDefaultGmm();

        if (gmm->gmmResourceInfo->getNumSamples() > 1) {
            return nullptr;
        }
        surfaceInfo = new SurfaceInfo();
        surfaceInfo->address = gmmHelper.decanonize(gfxAllocation.getGpuAddress());
        surfaceInfo->width = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth());
        surfaceInfo->height = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight());
        surfaceInfo->pitch = static_cast<uint32_t>(gmm->gmmResourceInfo->getRenderPitch());
        surfaceInfo->format = gmm->gmmResourceInfo->getResourceFormatSurfaceState();
        surfaceInfo->tilingType = gmm->gmmResourceInfo->getTileModeSurfaceState();
        surfaceInfo->surftype = getImageSurfaceTypeFromGmmResourceType<GfxFamily>(gmm->gmmResourceInfo->getResourceType());
        surfaceInfo->compressed = gfxAllocation.isCompressionEnabled();
        surfaceInfo->dumpType = (AubAllocDump::DumpFormat::IMAGE_TRE == dumpFormat) ? dumpType::tre : dumpType::bmp;
    }

    return surfaceInfo;
}
} // namespace AubAllocDump
