/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_properties.h"
#include "shared/source/utilities/lookup_array.h"

namespace NEO {

template <typename GfxFamily>
template <typename CommandType>
void BlitCommandsHelper<GfxFamily>::applyAdditionalBlitProperties(const BlitProperties &blitProperties,
                                                                  CommandType &cmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last) {
}

template <typename GfxFamily>
BlitCommandsResult BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    auto blitCmd = GfxFamily::cmdInitXyColorBlt;
    const auto maxWidth = getMaxBlitWidth(rootDeviceEnvironment);
    const auto maxHeight = getMaxBlitHeight(rootDeviceEnvironment, true);

    auto colorDepth = COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR;
    size_t patternSize = 16;

    const LookupArray<size_t, COLOR_DEPTH, 4> colorDepthLookup({{
        {1, COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR},
        {2, COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR},
        {4, COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR},
        {8, COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR},
    }});

    auto colorDepthV = colorDepthLookup.find(blitProperties.fillPatternSize);
    if (colorDepthV.has_value()) {
        colorDepth = *colorDepthV;
        patternSize = blitProperties.fillPatternSize;
    }

    blitCmd.setFillColor(blitProperties.fillPattern);
    blitCmd.setColorDepth(colorDepth);
    const bool hasAdditionalBlitProperties = rootDeviceEnvironment.getHelper<ProductHelper>().useAdditionalBlitProperties();

    BlitCommandsResult result{};
    bool firstCommand = true;
    uint64_t sizeToFill = blitProperties.copySize.x / patternSize;
    uint64_t offset = blitProperties.dstOffset.x;
    while (sizeToFill != 0) {
        auto tmpCmd = blitCmd;
        tmpCmd.setDestinationBaseAddress(ptrOffset(blitProperties.dstGpuAddress, static_cast<size_t>(offset)));
        uint64_t height = 0;
        uint64_t width = 0;
        if (sizeToFill <= maxWidth) {
            width = sizeToFill;
            height = 1;
        } else {
            width = maxWidth;
            height = std::min((sizeToFill / width), maxHeight);
            if (height > 1) {
                appendTilingEnable(tmpCmd);
            }
        }
        auto blitSize = width * height;
        auto lastCommand = (sizeToFill - blitSize == 0);
        tmpCmd.setDestinationX2CoordinateRight(static_cast<uint32_t>(width));
        tmpCmd.setDestinationY2CoordinateBottom(static_cast<uint32_t>(height));
        tmpCmd.setDestinationPitch(static_cast<uint32_t>(width * patternSize));

        if (blitProperties.dstAllocation) {
            appendBlitMemoryOptionsForFillBuffer(blitProperties.dstAllocation, tmpCmd, rootDeviceEnvironment);
        }
        appendBlitFillCommand(blitProperties, tmpCmd);

        if (hasAdditionalBlitProperties && (firstCommand || lastCommand)) {
            applyAdditionalBlitProperties(blitProperties, tmpCmd, rootDeviceEnvironment, lastCommand);
            firstCommand = false;
        }
        auto cmd = linearStream.getSpaceForCmd<XY_COLOR_BLT>();
        *cmd = tmpCmd;
        if (lastCommand) {
            result.lastBlitCommand = cmd;
        }
        offset += (blitSize * patternSize);
        sizeToFill -= blitSize;
    }
    return result;
}

} // namespace NEO
