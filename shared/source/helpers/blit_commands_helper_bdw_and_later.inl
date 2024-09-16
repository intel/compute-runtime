/*
 * Copyright (C) 2020-2024 Intel Corporation
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
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    return 0;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);
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
void BlitCommandsHelper<GfxFamily>::getBlitAllocationProperties(const GraphicsAllocation &allocation, uint32_t &pitch, uint32_t &qPitch,
                                                                GMM_TILE_TYPE &tileType, uint32_t &mipTailLod, uint32_t &compressionDetails,
                                                                const RootDeviceEnvironment &rootDeviceEnvironment, GMM_YUV_PLANE_ENUM plane) {
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
void BlitCommandsHelper<GfxFamily>::printImageBlitBlockCopyCommand(const typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const uint32_t sliceIndex) {}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchDummyBlit(LinearStream &linearStream, EncodeDummyBlitWaArgs &waArgs) {}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::isDummyBlitWaNeeded(const EncodeDummyBlitWaArgs &waArgs) {
    return false;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getDummyBlitSize(const EncodeDummyBlitWaArgs &waArgs) {
    return 0u;
}
} // namespace NEO
