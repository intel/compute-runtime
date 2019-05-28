/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/blit_commands_helper.h"

namespace NEO {

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(uint64_t copySize, CsrDependencies &csrDependencies) {
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

    size_t size = TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(csrDependencies) +
                  (sizeof(typename GfxFamily::XY_COPY_BLT) * numberOfBlits) +
                  sizeof(typename GfxFamily::MI_FLUSH_DW) +
                  sizeof(typename GfxFamily::MI_BATCH_BUFFER_END);

    return alignUp(size, MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBuffer(Buffer &dstBuffer, Buffer &srcBuffer, LinearStream &linearStream,
                                                                  uint64_t dstOffset, uint64_t srcOffset, uint64_t copySize) {
    uint64_t sizeToBlit = copySize;
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

        bltCmd->setDestinationBaseAddress(dstBuffer.getGraphicsAllocation()->getGpuAddress() + dstOffset + offset);
        bltCmd->setSourceBaseAddress(srcBuffer.getGraphicsAllocation()->getGpuAddress() + srcOffset + offset);

        appendBlitCommandsForBuffer(dstBuffer, srcBuffer, *bltCmd);

        auto blitSize = width * height;
        sizeToBlit -= blitSize;
        offset += blitSize;
    }
}

} // namespace NEO
