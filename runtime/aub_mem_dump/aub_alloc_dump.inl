/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_alloc_dump.h"
#include "runtime/gmm_helper/gmm.h"

using namespace NEO;

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
void dumpBufferInBinFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context) {
    AubMemDump::AubCaptureBinaryDumpHD cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Header.Type = 0x7;
    cmd.Header.Opcode = 0x1;
    cmd.Header.SubOp = 0x15;
    cmd.Header.DwordLength = ((sizeof(cmd) - sizeof(cmd.Header)) / sizeof(uint32_t)) - 1;

    cmd.setHeight(1);
    cmd.setWidth(gfxAllocation.getUnderlyingBufferSize());
    cmd.setBaseAddr(gfxAllocation.getGpuAddress());
    cmd.setPitch(gfxAllocation.getUnderlyingBufferSize());
    cmd.GttType = 1;
    cmd.DirectoryHandle = context;

    stream->write(reinterpret_cast<char *>(&cmd), sizeof(cmd));
}

template <typename GfxFamily>
void dumpImageInBmpFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context) {
    auto gmm = gfxAllocation.getDefaultGmm();

    AubMemDump::AubCmdDumpBmpHd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Header.Type = 0x7;
    cmd.Header.Opcode = 0x1;
    cmd.Header.SubOp = 0x44;
    cmd.Header.DwordLength = ((sizeof(cmd) - sizeof(cmd.Header)) / sizeof(uint32_t)) - 1;

    cmd.Xmin = 0;
    cmd.Ymin = 0;
    auto pitch = gmm->gmmResourceInfo->getRenderPitch();
    auto bitsPerPixel = gmm->gmmResourceInfo->getBitsPerPixel();
    auto pitchInPixels = static_cast<uint32_t>(8 * pitch / bitsPerPixel);
    cmd.BufferPitch = pitchInPixels;
    cmd.BitsPerPixel = bitsPerPixel;
    cmd.Format = gmm->gmmResourceInfo->getResourceFormatSurfaceState();
    cmd.Xsize = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth());
    cmd.Ysize = static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight());
    cmd.setBaseAddr(gfxAllocation.getGpuAddress());
    cmd.Secure = 0;
    cmd.UseFence = 0;
    auto flagInfo = gmm->gmmResourceInfo->getResourceFlags()->Info;
    cmd.TileOn = flagInfo.TiledW || flagInfo.TiledX || flagInfo.TiledY || flagInfo.TiledYf || flagInfo.TiledYs;
    cmd.WalkY = flagInfo.TiledY;
    cmd.UsePPGTT = 1;
    cmd.Use32BitDump = 1; // Dump out in 32bpp vs 24bpp
    cmd.UseFullFormat = 1;
    cmd.DirectoryHandle = context;

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
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    auto gmm = gfxAllocation.getDefaultGmm();
    if ((gmm->gmmResourceInfo->getNumSamples() > 1) || (gmm->isRenderCompressed)) {
        DEBUG_BREAK_IF(true); //unsupported
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
