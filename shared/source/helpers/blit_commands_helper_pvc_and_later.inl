/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_commands_helper.h"

namespace NEO {

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForByteFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    auto maxWidthToFill = getMaxBlitSetWidth(rootDeviceEnvironment);
    auto maxHeightToFill = getMaxBlitSetHeight(rootDeviceEnvironment);
    auto nBlits = 0;
    uint64_t width = 1;
    uint64_t height = 1;
    uint64_t sizeToFill = copySize.x / patternSize;
    while (sizeToFill != 0) {
        if (sizeToFill <= maxWidthToFill) {
            width = sizeToFill;
            height = 1;
        } else {
            width = maxWidthToFill;
            height = std::min((sizeToFill / width), maxHeightToFill);
        }
        sizeToFill -= (width * height);
        nBlits++;
    }
    return nBlits;
}

template <typename GfxFamily>
BlitCommandsResult BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryByteFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    using MEM_SET = typename Family::MEM_SET;
    auto blitCmd = Family::cmdInitMemSet;
    const auto maxBlitSetWidth = getMaxBlitSetWidth(rootDeviceEnvironment);
    const auto maxBlitSetHeight = getMaxBlitSetHeight(rootDeviceEnvironment);

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getL3EnabledMOCS();
    if (debugManager.flags.OverrideBlitterMocs.get() != -1) {
        mocs = static_cast<uint32_t>(debugManager.flags.OverrideBlitterMocs.get());
    }
    blitCmd.setDestinationMOCS(mocs);

    uint32_t compressionFormat = 0;

    if (blitProperties.dstAllocation) {
        if (blitProperties.dstAllocation->isCompressionEnabled()) {
            auto resourceFormat = blitProperties.dstAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
            compressionFormat = static_cast<uint32_t>(rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat));
        }

        appendBlitMemSetCompressionFormat(&blitCmd, blitProperties.dstAllocation, compressionFormat);
    }
    blitCmd.setFillData(*blitProperties.fillPattern);

    const bool useAdditionalBlitProperties = rootDeviceEnvironment.getHelper<ProductHelper>().useAdditionalBlitProperties();

    uint64_t sizeToFill = blitProperties.copySize.x;
    uint64_t offset = blitProperties.dstOffset.x;
    BlitCommandsResult result{};
    bool firstCommand = true;
    while (sizeToFill != 0) {
        auto tmpCmd = blitCmd;
        tmpCmd.setDestinationStartAddress(ptrOffset(blitProperties.dstGpuAddress, static_cast<size_t>(offset)));
        uint64_t height = 0;
        uint64_t width = 0;
        if (sizeToFill <= maxBlitSetWidth) {
            width = sizeToFill;
            height = 1;
        } else {
            width = maxBlitSetWidth;
            height = std::min<uint64_t>((sizeToFill / width), maxBlitSetHeight);
            if (height > 1) {
                tmpCmd.setFillType(MEM_SET::FILL_TYPE::FILL_TYPE_MATRIX_FILL);
            }
        }
        auto blitSize = static_cast<uint64_t>(width * height);
        auto lastCommand = (sizeToFill - blitSize == 0);
        tmpCmd.setFillWidth(static_cast<uint32_t>(width));
        tmpCmd.setFillHeight(static_cast<uint32_t>(height));
        tmpCmd.setDestinationPitch(static_cast<uint32_t>(width));

        if (useAdditionalBlitProperties && (firstCommand || lastCommand)) {
            applyAdditionalBlitProperties(blitProperties, tmpCmd, rootDeviceEnvironment, lastCommand);
            firstCommand = false;
        }
        appendBlitMemSetCommand(blitProperties, &tmpCmd);

        auto cmd = linearStream.getSpaceForCmd<MEM_SET>();
        *cmd = tmpCmd;
        if (lastCommand) {
            result.lastBlitCommand = cmd;
        }
        offset += blitSize;
        sizeToFill -= blitSize;
    }
    return result;
}

} // namespace NEO
