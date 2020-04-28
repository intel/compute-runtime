/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper_base.inl"

namespace NEO {

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForBuffer(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForFillBuffer(NEO::GraphicsAllocation *dstAlloc, typename GfxFamily::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    if (blitCmd.getColorDepth() == XY_COLOR_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR) {
        blitCmd.set32BppByteMask(XY_COLOR_BLT::_32BPP_BYTE_MASK::_32BPP_BYTE_MASK_WRITE_RGBA_CHANNEL);
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(NEO::GraphicsAllocation *dstAlloc, uint32_t *pattern, size_t patternSize, LinearStream &linearStream, size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) {
    switch (patternSize) {
    case 1:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<1>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
        break;
    case 2:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<2>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR1555);
        break;
    default:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<4>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingEnable(typename GfxFamily::XY_COLOR_BLT &blitCmd) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    blitCmd.setDestTilingEnable(XY_COLOR_BLT::DEST_TILING_ENABLE::DEST_TILING_ENABLE_TILING_ENABLED);
}

} // namespace NEO
