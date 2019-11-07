/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/blit_commands_helper.h"

namespace NEO {

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(uint64_t copySize, const CsrDependencies &csrDependencies, bool updateTimestampPacket) {
    size_t numberOfBlits = 0;
    uint64_t sizeToBlit = copySize;
    uint64_t width = 1;
    uint64_t height = 1;

    while (sizeToBlit != 0) {
        if (sizeToBlit > BlitterConstants::maxBlitWidth) {
            // 2D: maxBlitWidth x (1 .. maxBlitHeight)
            width = BlitterConstants::maxBlitWidth;
            height = std::min((sizeToBlit / width), BlitterConstants::maxBlitHeight);
        } else {
            // 1D: (1 .. maxBlitWidth) x 1
            width = sizeToBlit;
            height = 1;
        }
        sizeToBlit -= (width * height);
        numberOfBlits++;
    }

    return TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(csrDependencies) +
           (sizeof(typename GfxFamily::XY_COPY_BLT) * numberOfBlits) +
           (sizeof(typename GfxFamily::MI_FLUSH_DW) * static_cast<size_t>(updateTimestampPacket));
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(const BlitPropertiesContainer &blitPropertiesContainer) {
    size_t size = 0;
    for (auto &blitProperties : blitPropertiesContainer) {
        size += BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(blitProperties.copySize, blitProperties.csrDependencies,
                                                                        blitProperties.outputTimestampPacket != nullptr);
    }
    size += sizeof(typename GfxFamily::MI_FLUSH_DW) + sizeof(typename GfxFamily::MI_BATCH_BUFFER_END);

    return alignUp(size, MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBuffer(const BlitProperties &blitProperties, LinearStream &linearStream) {
    uint64_t sizeToBlit = blitProperties.copySize;
    uint64_t width = 1;
    uint64_t height = 1;
    uint64_t offset = 0;

    while (sizeToBlit != 0) {
        if (sizeToBlit > BlitterConstants::maxBlitWidth) {
            // dispatch 2D blit: maxBlitWidth x (1 .. maxBlitHeight)
            width = BlitterConstants::maxBlitWidth;
            height = std::min((sizeToBlit / width), BlitterConstants::maxBlitHeight);
        } else {
            // dispatch 1D blt: (1 .. maxBlitWidth) x 1
            width = sizeToBlit;
            height = 1;
        }

        auto bltCmd = linearStream.getSpaceForCmd<typename GfxFamily::XY_COPY_BLT>();
        *bltCmd = GfxFamily::cmdInitXyCopyBlt;

        bltCmd->setDestinationX1CoordinateLeft(0);
        bltCmd->setDestinationY1CoordinateTop(0);
        bltCmd->setSourceX1CoordinateLeft(0);
        bltCmd->setSourceY1CoordinateTop(0);

        bltCmd->setDestinationX2CoordinateRight(static_cast<uint32_t>(width));
        bltCmd->setDestinationY2CoordinateBottom(static_cast<uint32_t>(height));

        bltCmd->setDestinationPitch(static_cast<uint32_t>(width));
        bltCmd->setSourcePitch(static_cast<uint32_t>(width));

        bltCmd->setDestinationBaseAddress(blitProperties.dstAllocation->getGpuAddress() + blitProperties.dstOffset + offset);
        bltCmd->setSourceBaseAddress(blitProperties.srcAllocation->getGpuAddress() + blitProperties.srcOffset + offset);

        appendBlitCommandsForBuffer(blitProperties, *bltCmd);

        auto blitSize = width * height;
        sizeToBlit -= blitSize;
        offset += blitSize;
    }
}

} // namespace NEO
