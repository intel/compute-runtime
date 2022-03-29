/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper_base.inl"

namespace NEO {

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitWidthOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return 0;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return 0;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForImages(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch) {
    appendTilingType(GMM_NOT_TILED, GMM_NOT_TILED, blitCmd);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForFillBuffer(NEO::GraphicsAllocation *dstAlloc, typename GfxFamily::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    if (blitCmd.getColorDepth() == XY_COLOR_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR) {
        blitCmd.set32BppByteMask(XY_COLOR_BLT::_32BPP_BYTE_MASK::_32BPP_BYTE_MASK_WRITE_RGBA_CHANNEL);
    }
    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(NEO::GraphicsAllocation *dstAlloc, uint64_t offset, uint32_t *pattern, size_t patternSize, LinearStream &linearStream, size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) {
    switch (patternSize) {
    case 1:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<1>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
        break;
    case 2:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<2>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR1555);
        break;
    default:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<4>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
}
template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingEnable(typename GfxFamily::XY_COLOR_BLT &blitCmd) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    blitCmd.setDestTilingEnable(XY_COLOR_BLT::DEST_TILING_ENABLE::DEST_TILING_ENABLE_TILING_ENABLED);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingType(const GMM_TILE_TYPE srcTilingType, const GMM_TILE_TYPE dstTilingType, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
}
template <typename GfxFamily>
template <typename T>
void BlitCommandsHelper<GfxFamily>::appendColorDepth(const BlitProperties &blitProperites, T &blitCmd) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSliceOffsets(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, uint32_t sliceIndex, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t srcSlicePitch, uint32_t dstSlicePitch) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::getBlitAllocationProperties(const GraphicsAllocation &allocation, uint32_t &pitch, uint32_t &qPitch, GMM_TILE_TYPE &tileType, uint32_t &mipTailLod, uint32_t &compressionDetails, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::programGlobalSequencerFlush(LinearStream &commandStream) {
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getSizeForGlobalSequencerFlush() {
    return 0u;
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::miArbCheckWaRequired() {
    return false;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendClearColor(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::printImageBlitBlockCopyCommand(const typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {}

} // namespace NEO
