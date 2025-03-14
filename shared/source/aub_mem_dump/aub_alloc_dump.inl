/*
 * Copyright (C) 2019-2024 Intel Corporation
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

template <typename GfxFamily>
void dumpBufferInBinFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context) {
    AubMemDump::AubCaptureBinaryDumpHD cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.header.type = 0x7;
    cmd.header.opcode = 0x1;
    cmd.header.subOp = 0x15;
    cmd.header.dwordLength = ((sizeof(cmd) - sizeof(cmd.header)) / sizeof(uint32_t)) - 1;

    cmd.setHeight(1);
    cmd.setWidth(gfxAllocation.getUnderlyingBufferSize());
    cmd.setBaseAddr(gfxAllocation.getGpuAddress());
    cmd.setPitch(gfxAllocation.getUnderlyingBufferSize());
    cmd.gttType = 1;
    cmd.directoryHandle = context;

    stream->write(reinterpret_cast<char *>(&cmd), sizeof(cmd));
}

template <typename GfxFamily>
void dumpImageInBmpFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context) {
    auto gmm = gfxAllocation.getDefaultGmm();

    AubMemDump::AubCmdDumpBmpHd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.header.type = 0x7;
    cmd.header.opcode = 0x1;
    cmd.header.subOp = 0x44;
    cmd.header.dwordLength = ((sizeof(cmd) - sizeof(cmd.header)) / sizeof(uint32_t)) - 1;

    cmd.xMin = 0;
    cmd.yMin = 0;
    auto pitch = gmm->gmmResourceInfo->getRenderPitch();
    auto bitsPerPixel = gmm->gmmResourceInfo->getBitsPerPixel();
    auto pitchInPixels = static_cast<uint32_t>(8 * pitch / bitsPerPixel);
    cmd.bufferPitch = pitchInPixels;
    cmd.bitsPerPixel = bitsPerPixel;
    cmd.format = gmm->gmmResourceInfo->getResourceFormatSurfaceState();
    cmd.xSize = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth());
    cmd.ySize = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight());
    cmd.setBaseAddr(gfxAllocation.getGpuAddress());
    cmd.secure = 0;
    cmd.useFence = 0;
    auto flagInfo = gmm->gmmResourceInfo->getResourceFlags()->Info;
    cmd.tileOn = flagInfo.TiledW || flagInfo.TiledX || flagInfo.TiledY || flagInfo.TiledYf || flagInfo.TiledYs;
    cmd.walkY = flagInfo.TiledY;
    cmd.usePPGTT = 1;
    cmd.use32BitDump = 1; // Dump out in 32bpp vs 24bpp
    cmd.useFullFormat = 1;
    cmd.directoryHandle = context;

    stream->write(reinterpret_cast<char *>(&cmd), sizeof(cmd));
}

template <typename GfxFamily>
void dumpBufferInTreFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    AubMemDump::CmdServicesMemTraceDumpCompress cmd;
    memset(&cmd, 0, sizeof(AubMemDump::CmdServicesMemTraceDumpCompress));

    cmd.dwordCount = (sizeof(AubMemDump::CmdServicesMemTraceDumpCompress) - 1) / 4;
    cmd.instructionSubOpcode = 0x10;
    cmd.instructionOpcode = 0x2e;
    cmd.instructionType = 0x7;

    cmd.setSurfaceAddress(gfxAllocation.getGpuAddress());
    cmd.surfaceWidth = static_cast<uint32_t>(gfxAllocation.getUnderlyingBufferSize());
    cmd.surfaceHeight = 1;
    cmd.surfacePitch = static_cast<uint32_t>(gfxAllocation.getUnderlyingBufferSize());
    cmd.surfaceFormat = SURFACE_FORMAT::SURFACE_FORMAT_RAW;
    cmd.dumpType = AubMemDump::CmdServicesMemTraceDumpCompress::DumpTypeValues::Tre;
    cmd.surfaceTilingType = RENDER_SURFACE_STATE::TILE_MODE_LINEAR;
    cmd.surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;

    cmd.algorithm = AubMemDump::CmdServicesMemTraceDumpCompress::AlgorithmValues::Uncompressed;

    cmd.gttType = 1;
    cmd.directoryHandle = context;

    stream->write(reinterpret_cast<char *>(&cmd), sizeof(cmd));
}

template <typename GfxFamily>
void dumpImageInTreFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context) {
    auto gmm = gfxAllocation.getDefaultGmm();
    if ((gmm->gmmResourceInfo->getNumSamples() > 1) || (gfxAllocation.isCompressionEnabled())) {
        DEBUG_BREAK_IF(true); // unsupported
        return;
    }

    auto surfaceType = getImageSurfaceTypeFromGmmResourceType<GfxFamily>(gmm->gmmResourceInfo->getResourceType());

    AubMemDump::CmdServicesMemTraceDumpCompress cmd;
    memset(&cmd, 0, sizeof(AubMemDump::CmdServicesMemTraceDumpCompress));

    cmd.dwordCount = (sizeof(AubMemDump::CmdServicesMemTraceDumpCompress) - 1) / 4;
    cmd.instructionSubOpcode = 0x10;
    cmd.instructionOpcode = 0x2e;
    cmd.instructionType = 0x7;

    cmd.setSurfaceAddress(gfxAllocation.getGpuAddress());
    cmd.surfaceWidth = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth());
    cmd.surfaceHeight = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight());
    cmd.surfacePitch = static_cast<uint32_t>(gmm->gmmResourceInfo->getRenderPitch());
    cmd.surfaceFormat = gmm->gmmResourceInfo->getResourceFormatSurfaceState();
    cmd.dumpType = AubMemDump::CmdServicesMemTraceDumpCompress::DumpTypeValues::Tre;
    cmd.surfaceTilingType = gmm->gmmResourceInfo->getTileModeSurfaceState();
    cmd.surfaceType = surfaceType;

    cmd.algorithm = AubMemDump::CmdServicesMemTraceDumpCompress::AlgorithmValues::Uncompressed;

    cmd.gttType = 1;
    cmd.directoryHandle = context;

    stream->write(reinterpret_cast<char *>(&cmd), sizeof(cmd));
}

template <typename GfxFamily>
void dumpAllocation(DumpFormat dumpFormat, GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context) {
    switch (dumpFormat) {
    case DumpFormat::BUFFER_BIN:
        dumpBufferInBinFormat<GfxFamily>(gfxAllocation, stream, context);
        break;
    case DumpFormat::BUFFER_TRE:
        dumpBufferInTreFormat<GfxFamily>(gfxAllocation, stream, context);
        break;
    case DumpFormat::IMAGE_BMP:
        dumpImageInBmpFormat<GfxFamily>(gfxAllocation, stream, context);
        break;
    case DumpFormat::IMAGE_TRE:
        dumpImageInTreFormat<GfxFamily>(gfxAllocation, stream, context);
        break;
    default:
        break;
    }
}
} // namespace AubAllocDump
