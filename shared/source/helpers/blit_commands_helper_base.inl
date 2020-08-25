/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/timestamp_packet.h"

namespace NEO {

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitWidth(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (DebugManager.flags.LimitBlitterMaxWidth.get() != -1) {
        return static_cast<uint64_t>(DebugManager.flags.LimitBlitterMaxWidth.get());
    }
    auto maxBlitWidthOverride = getMaxBlitWidthOverride(rootDeviceEnvironment);
    if (maxBlitWidthOverride > 0) {
        return maxBlitWidthOverride;
    }
    return BlitterConstants::maxBlitWidth;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeight(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (DebugManager.flags.LimitBlitterMaxHeight.get() != -1) {
        return static_cast<uint64_t>(DebugManager.flags.LimitBlitterMaxHeight.get());
    }
    auto maxBlitHeightOverride = getMaxBlitHeightOverride(rootDeviceEnvironment);
    if (maxBlitHeightOverride > 0) {
        return maxBlitHeightOverride;
    }
    return BlitterConstants::maxBlitHeight;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchPostBlitCommand(LinearStream &linearStream) {
    bool useFlush = false;
    if (DebugManager.flags.FlushAfterEachBlit.get() != -1) {
        useFlush = static_cast<bool>(DebugManager.flags.FlushAfterEachBlit.get());
    }

    if (useFlush) {
        EncodeMiFlushDW<GfxFamily>::programMiFlushDw(linearStream, 0, 0, false, false);
    } else {
        auto miArbCheckStream = linearStream.getSpaceForCmd<typename GfxFamily::MI_ARB_CHECK>();
        *miArbCheckStream = GfxFamily::cmdInitArbCheck;
    }
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimatePostBlitCommandSize() {
    bool useFlush = false;
    if (DebugManager.flags.FlushAfterEachBlit.get() != -1) {
        useFlush = static_cast<bool>(DebugManager.flags.FlushAfterEachBlit.get());
    }

    if (useFlush) {
        return sizeof(typename GfxFamily::MI_FLUSH_DW);
    }
    return sizeof(typename GfxFamily::MI_ARB_CHECK);
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(const Vec3<size_t> &copySize, const CsrDependencies &csrDependencies,
                                                               bool updateTimestampPacket, bool profilingEnabled,
                                                               const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t timestampCmdSize = 0;
    if (updateTimestampPacket) {
        timestampCmdSize = (profilingEnabled) ? 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM)
                                              : EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    }

    bool preferRegionCopy = isCopyRegionPreferred(copySize, rootDeviceEnvironment);
    auto nBlits = preferRegionCopy ? getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment)
                                   : getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment);

    auto sizePerBlit = (sizeof(typename GfxFamily::XY_COPY_BLT) + estimatePostBlitCommandSize());

    return TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(csrDependencies) + (sizePerBlit * nBlits) + timestampCmdSize;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(const BlitPropertiesContainer &blitPropertiesContainer,
                                                               bool profilingEnabled, bool debugPauseEnabled,
                                                               const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t size = 0;
    for (auto &blitProperties : blitPropertiesContainer) {
        size += BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(blitProperties.copySize, blitProperties.csrDependencies,
                                                                        blitProperties.outputTimestampPacket != nullptr, profilingEnabled,
                                                                        rootDeviceEnvironment);
    }
    size += MemorySynchronizationCommands<GfxFamily>::getSizeForAdditonalSynchronization(*rootDeviceEnvironment.getHardwareInfo());
    size += EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    size += sizeof(typename GfxFamily::MI_BATCH_BUFFER_END);

    if (debugPauseEnabled) {
        size += BlitCommandsHelper<GfxFamily>::getSizeForDebugPauseCommands();
    }

    return alignUp(size, MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandDestinationBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice) {
    return blitProperties.dstGpuAddress + blitProperties.dstOffset.x + offset +
           blitProperties.dstOffset.y * blitProperties.dstRowPitch +
           blitProperties.dstOffset.z * blitProperties.dstSlicePitch +
           row * blitProperties.dstRowPitch +
           slice * blitProperties.dstSlicePitch;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandSourceBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice) {
    return blitProperties.srcGpuAddress + blitProperties.srcOffset.x + offset +
           blitProperties.srcOffset.y * blitProperties.srcRowPitch +
           blitProperties.srcOffset.z * blitProperties.srcSlicePitch +
           row * blitProperties.srcRowPitch +
           slice * blitProperties.srcSlicePitch;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferPerRow(const BlitProperties &blitProperties, LinearStream &linearStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
    uint64_t width = 1;
    uint64_t height = 1;

    for (uint64_t slice = 0; slice < blitProperties.copySize.z; slice++) {
        for (uint64_t row = 0; row < blitProperties.copySize.y; row++) {
            uint64_t offset = 0;
            uint64_t sizeToBlit = blitProperties.copySize.x;
            while (sizeToBlit != 0) {
                if (sizeToBlit > getMaxBlitWidth(rootDeviceEnvironment)) {
                    // dispatch 2D blit: maxBlitWidth x (1 .. maxBlitHeight)
                    width = getMaxBlitWidth(rootDeviceEnvironment);
                    height = std::min((sizeToBlit / width), getMaxBlitHeight(rootDeviceEnvironment));
                } else {
                    // dispatch 1D blt: (1 .. maxBlitWidth) x 1
                    width = sizeToBlit;
                    height = 1;
                }

                {
                    auto bltCmd = GfxFamily::cmdInitXyCopyBlt;

                    bltCmd.setTransferWidth(static_cast<uint32_t>(width));
                    bltCmd.setTransferHeight(static_cast<uint32_t>(height));
                    bltCmd.setDestinationPitch(static_cast<uint32_t>(width));
                    bltCmd.setSourcePitch(static_cast<uint32_t>(width));

                    auto dstAddr = calculateBlitCommandDestinationBaseAddress(blitProperties, offset, row, slice);
                    auto srcAddr = calculateBlitCommandSourceBaseAddress(blitProperties, offset, row, slice);

                    bltCmd.setDestinationBaseAddress(dstAddr);
                    bltCmd.setSourceBaseAddress(srcAddr);

                    appendBlitCommandsForBuffer(blitProperties, bltCmd, rootDeviceEnvironment);

                    auto bltStream = linearStream.getSpaceForCmd<typename GfxFamily::XY_COPY_BLT>();
                    *bltStream = bltCmd;
                }

                dispatchPostBlitCommand(linearStream);

                auto blitSize = width * height;
                sizeToBlit -= blitSize;
                offset += blitSize;
            }
        }
    }
}

template <typename GfxFamily>
template <size_t patternSize>
void BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill(NEO::GraphicsAllocation *dstAlloc, uint32_t *pattern, LinearStream &linearStream, size_t size, const RootDeviceEnvironment &rootDeviceEnvironment, COLOR_DEPTH depth) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    auto blitCmd = GfxFamily::cmdInitXyColorBlt;

    blitCmd.setFillColor(pattern);
    blitCmd.setColorDepth(depth);

    uint64_t offset = 0;
    uint64_t sizeToFill = size;
    while (sizeToFill != 0) {
        auto tmpCmd = blitCmd;
        tmpCmd.setDestinationBaseAddress(ptrOffset(dstAlloc->getGpuAddress(), static_cast<size_t>(offset)));
        uint64_t height = 0;
        uint64_t width = 0;
        if (sizeToFill <= getMaxBlitWidth(rootDeviceEnvironment)) {
            width = sizeToFill;
            height = 1;
        } else {
            width = getMaxBlitWidth(rootDeviceEnvironment);
            height = std::min((sizeToFill / width), getMaxBlitHeight(rootDeviceEnvironment));
            if (height > 1) {
                appendTilingEnable(tmpCmd);
            }
        }
        tmpCmd.setTransferWidth(static_cast<uint32_t>(width));
        tmpCmd.setTransferHeight(static_cast<uint32_t>(height));
        tmpCmd.setDestinationPitch(static_cast<uint32_t>(width));

        appendBlitCommandsForFillBuffer(dstAlloc, tmpCmd, rootDeviceEnvironment);

        auto cmd = linearStream.getSpaceForCmd<XY_COLOR_BLT>();
        *cmd = tmpCmd;
        auto blitSize = width * height;
        offset += (blitSize);
        sizeToFill -= blitSize;
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsRegion(const BlitProperties &blitProperties, LinearStream &linearStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto dstAllocation = blitProperties.dstAllocation;
    auto srcAllocation = blitProperties.srcAllocation;

    UNRECOVERABLE_IF(blitProperties.copySize.x > BlitterConstants::maxBlitWidth || blitProperties.copySize.y > BlitterConstants::maxBlitWidth);
    auto bltCmd = GfxFamily::cmdInitXyCopyBlt;

    bltCmd.setSourceBaseAddress(srcAllocation->getGpuAddress());
    bltCmd.setDestinationBaseAddress(dstAllocation->getGpuAddress());

    bltCmd.setDestinationX1CoordinateLeft(static_cast<uint32_t>(blitProperties.dstOffset.x));
    bltCmd.setDestinationY1CoordinateTop(static_cast<uint32_t>(blitProperties.dstOffset.y));
    bltCmd.setTransferWidth(static_cast<uint32_t>(blitProperties.dstOffset.x + blitProperties.copySize.x));
    bltCmd.setTransferHeight(static_cast<uint32_t>(blitProperties.dstOffset.y + blitProperties.copySize.y));

    bltCmd.setSourceX1CoordinateLeft(static_cast<uint32_t>(blitProperties.srcOffset.x));
    bltCmd.setSourceY1CoordinateTop(static_cast<uint32_t>(blitProperties.srcOffset.y));

    appendBlitCommandsForBuffer(blitProperties, bltCmd, rootDeviceEnvironment);
    appendBlitCommandsForImages(blitProperties, bltCmd);
    appendColorDepth(blitProperties, bltCmd);
    appendSurfaceType(blitProperties, bltCmd);
    for (uint32_t i = 0; i < blitProperties.copySize.z; i++) {
        appendSliceOffsets(blitProperties, bltCmd, i);
        auto cmd = linearStream.getSpaceForCmd<typename GfxFamily::XY_COPY_BLT>();
        *cmd = bltCmd;
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(LinearStream &commandStream, uint64_t debugPauseStateGPUAddress, DebugPauseState confirmationTrigger, DebugPauseState waitCondition) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, debugPauseStateGPUAddress, static_cast<uint32_t>(confirmationTrigger), false, true);

    auto miSemaphoreCmd = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    EncodeSempahore<GfxFamily>::programMiSemaphoreWait(miSemaphoreCmd, debugPauseStateGPUAddress, static_cast<uint32_t>(waitCondition), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD);
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getSizeForDebugPauseCommands() {
    return (EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite() + EncodeSempahore<GfxFamily>::getSizeMiSemaphoreWait()) * 2;
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::useOneBlitCopyCommand(Vec3<size_t> copySize, uint32_t bytesPerPixel) {
    return (copySize.x / bytesPerPixel <= BlitterConstants::maxBlitWidth && copySize.y <= BlitterConstants::maxBlitHeight);
}

template <typename GfxFamily>
uint32_t BlitCommandsHelper<GfxFamily>::getAvailableBytesPerPixel(size_t copySize, uint32_t srcOrigin, uint32_t dstOrigin, uint32_t srcSize, uint32_t dstSize) {
    uint32_t bytesPerPixel = BlitterConstants::maxBytesPerPixel;
    while (bytesPerPixel > 1) {
        if (copySize % bytesPerPixel == 0 && srcSize % bytesPerPixel == 0 && dstSize % bytesPerPixel == 0) {
            if ((srcOrigin ? (srcOrigin % bytesPerPixel == 0) : true) && (dstOrigin ? (dstOrigin % bytesPerPixel == 0) : true)) {
                break;
            }
        }
        bytesPerPixel >>= 1;
    }
    return bytesPerPixel;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitCommands(const BlitProperties &blitProperties, LinearStream &linearStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
    bool preferCopyRegion = isCopyRegionPreferred(blitProperties.copySize, rootDeviceEnvironment);

    preferCopyRegion ? dispatchBlitCommandsForBufferRegion(blitProperties, linearStream, rootDeviceEnvironment)
                     : dispatchBlitCommandsForBufferPerRow(blitProperties, linearStream, rootDeviceEnvironment);
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandSourceBaseAddressCopyRegion(const BlitProperties &blitProperties, size_t slice) {
    return blitProperties.srcGpuAddress + blitProperties.srcOffset.x +
           (blitProperties.srcOffset.y * blitProperties.srcRowPitch) +
           (blitProperties.srcSlicePitch * (slice + blitProperties.srcOffset.z));
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandDestinationBaseAddressCopyRegion(const BlitProperties &blitProperties, size_t slice) {
    return blitProperties.dstGpuAddress + blitProperties.dstOffset.x +
           (blitProperties.dstOffset.y * blitProperties.dstRowPitch) +
           (blitProperties.dstSlicePitch * (slice + blitProperties.dstOffset.z));
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferRegion(const BlitProperties &blitProperties, LinearStream &linearStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto maxWidthToCopy = getMaxBlitWidth(rootDeviceEnvironment);
    const auto maxHeightToCopy = getMaxBlitHeight(rootDeviceEnvironment);

    for (size_t slice = 0u; slice < blitProperties.copySize.z; ++slice) {
        auto srcAddress = calculateBlitCommandSourceBaseAddressCopyRegion(blitProperties, slice);
        auto dstAddress = calculateBlitCommandDestinationBaseAddressCopyRegion(blitProperties, slice);
        auto heightToCopy = blitProperties.copySize.y;

        while (heightToCopy > 0) {
            auto height = static_cast<uint32_t>(std::min(heightToCopy, static_cast<size_t>(maxHeightToCopy)));
            auto widthToCopy = blitProperties.copySize.x;

            while (widthToCopy > 0) {
                auto width = static_cast<uint32_t>(std::min(widthToCopy, static_cast<size_t>(maxWidthToCopy)));
                auto bltCmd = GfxFamily::cmdInitXyCopyBlt;

                bltCmd.setSourceBaseAddress(srcAddress);
                bltCmd.setDestinationBaseAddress(dstAddress);
                bltCmd.setTransferWidth(width);
                bltCmd.setTransferHeight(height);
                bltCmd.setSourcePitch(static_cast<uint32_t>(blitProperties.srcRowPitch));
                bltCmd.setDestinationPitch(static_cast<uint32_t>(blitProperties.dstRowPitch));

                appendBlitCommandsForBuffer(blitProperties, bltCmd, rootDeviceEnvironment);

                auto cmd = linearStream.getSpaceForCmd<typename GfxFamily::XY_COPY_BLT>();
                *cmd = bltCmd;
                dispatchPostBlitCommand(linearStream);

                srcAddress += width;
                dstAddress += width;
                widthToCopy -= width;
            }

            heightToCopy -= height;
            srcAddress += (blitProperties.srcRowPitch - blitProperties.copySize.x);
            srcAddress += (blitProperties.srcRowPitch * (height - 1));
            dstAddress += (blitProperties.dstRowPitch - blitProperties.copySize.x);
            dstAddress += (blitProperties.dstRowPitch * (height - 1));
        }
    }
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::isCopyRegionPreferred(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment) {
    bool preferCopyRegion = getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment) < getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment);
    return preferCopyRegion;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForCopyRegion(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto maxWidthToCopy = getMaxBlitWidth(rootDeviceEnvironment);
    auto maxHeightToCopy = getMaxBlitHeight(rootDeviceEnvironment);
    auto xBlits = static_cast<size_t>(std::ceil(copySize.x / static_cast<double>(maxWidthToCopy)));
    auto yBlits = static_cast<size_t>(std::ceil(copySize.y / static_cast<double>(maxHeightToCopy)));
    auto zBlits = static_cast<size_t>(copySize.z);
    auto nBlits = xBlits * yBlits * zBlits;

    return nBlits;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForCopyPerRow(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t xBlits = 0u;
    uint64_t width = 1;
    uint64_t height = 1;
    uint64_t sizeToBlit = copySize.x;

    while (sizeToBlit != 0) {
        if (sizeToBlit > getMaxBlitWidth(rootDeviceEnvironment)) {
            // dispatch 2D blit: maxBlitWidth x (1 .. maxBlitHeight)
            width = getMaxBlitWidth(rootDeviceEnvironment);
            height = std::min((sizeToBlit / width), getMaxBlitHeight(rootDeviceEnvironment));

        } else {
            // dispatch 1D blt: (1 .. maxBlitWidth) x 1
            width = sizeToBlit;
            height = 1;
        }
        sizeToBlit -= (width * height);
        xBlits++;
    }

    auto yBlits = copySize.y;
    auto zBlits = copySize.z;
    auto nBlits = xBlits * yBlits * zBlits;

    return nBlits;
}

} // namespace NEO
