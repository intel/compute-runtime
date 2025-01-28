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
void BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryByteFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    using MEM_SET = typename Family::MEM_SET;
    auto blitCmd = Family::cmdInitMemSet;

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    if (debugManager.flags.OverrideBlitterMocs.get() != -1) {
        mocs = static_cast<uint32_t>(debugManager.flags.OverrideBlitterMocs.get());
    }
    blitCmd.setDestinationMOCS(mocs);

    uint32_t compressionFormat = 0;
    if (blitProperties.dstAllocation->isCompressionEnabled()) {
        auto resourceFormat = blitProperties.dstAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        compressionFormat = static_cast<uint32_t>(rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat));
    }

    appendBlitMemSetCompressionFormat(&blitCmd, blitProperties.dstAllocation, compressionFormat);

    blitCmd.setFillData(*blitProperties.fillPattern);

    auto sizeToFill = blitProperties.copySize.x;
    uint64_t offset = blitProperties.dstOffset.x;
    while (sizeToFill != 0) {
        auto tmpCmd = blitCmd;
        tmpCmd.setDestinationStartAddress(ptrOffset(blitProperties.dstAllocation->getGpuAddress(), static_cast<size_t>(offset)));
        size_t height = 0;
        size_t width = 0;
        if (sizeToFill <= BlitterConstants::maxBlitSetWidth) {
            width = sizeToFill;
            height = 1;
        } else {
            width = BlitterConstants::maxBlitSetWidth;
            height = std::min<size_t>((sizeToFill / width), BlitterConstants::maxBlitSetHeight);
            if (height > 1) {
                tmpCmd.setFillType(MEM_SET::FILL_TYPE::FILL_TYPE_MATRIX_FILL);
            }
        }
        tmpCmd.setFillWidth(static_cast<uint32_t>(width));
        tmpCmd.setFillHeight(static_cast<uint32_t>(height));
        tmpCmd.setDestinationPitch(static_cast<uint32_t>(width));

        appendBlitMemSetCommand(blitProperties, &tmpCmd);

        auto cmd = linearStream.getSpaceForCmd<MEM_SET>();
        *cmd = tmpCmd;

        auto blitSize = width * height;
        offset += blitSize;
        sizeToFill -= blitSize;
    }
}

} // namespace NEO
