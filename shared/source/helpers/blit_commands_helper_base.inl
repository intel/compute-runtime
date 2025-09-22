/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/lookup_array.h"

#include <cmath>

namespace NEO {

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitWidth(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.LimitBlitterMaxWidth.get() != -1) {
        return static_cast<uint64_t>(debugManager.flags.LimitBlitterMaxWidth.get());
    }
    auto maxBlitWidthOverride = getMaxBlitWidthOverride(rootDeviceEnvironment);
    if (maxBlitWidthOverride > 0) {
        return maxBlitWidthOverride;
    }
    return BlitterConstants::maxBlitWidth;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeight(const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    if (debugManager.flags.LimitBlitterMaxHeight.get() != -1) {
        return static_cast<uint64_t>(debugManager.flags.LimitBlitterMaxHeight.get());
    }
    auto maxBlitHeightOverride = getMaxBlitHeightOverride(rootDeviceEnvironment, isSystemMemoryPoolUsed);
    if (maxBlitHeightOverride > 0) {
        return maxBlitHeightOverride;
    }
    return BlitterConstants::maxBlitHeight;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitSetWidth(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.LimitBlitterMaxSetWidth.get() != -1) {
        return static_cast<uint64_t>(debugManager.flags.LimitBlitterMaxSetWidth.get());
    }
    return BlitterConstants::maxBlitSetWidth;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitSetHeight(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.LimitBlitterMaxSetHeight.get() != -1) {
        return static_cast<uint64_t>(debugManager.flags.LimitBlitterMaxSetHeight.get());
    }
    return BlitterConstants::maxBlitSetHeight;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchPreBlitCommand(LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    if (BlitCommandsHelper<GfxFamily>::preBlitCommandWARequired()) {
        NEO::EncodeDummyBlitWaArgs waArgs{false, &rootDeviceEnvironment};
        MiFlushArgs args{waArgs};

        EncodeMiFlushDW<GfxFamily>::programWithWa(linearStream, 0, 0, args);
    }
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimatePreBlitCommandSize() {
    if (BlitCommandsHelper<GfxFamily>::preBlitCommandWARequired()) {
        return EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa({});
    }

    return 0u;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchPostBlitCommand(LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    EncodeDummyBlitWaArgs waArgs{false, &rootDeviceEnvironment};
    MiFlushArgs args{waArgs};
    if (debugManager.flags.PostBlitCommand.get() != BlitterConstants::PostBlitMode::defaultMode) {
        switch (debugManager.flags.PostBlitCommand.get()) {
        case BlitterConstants::PostBlitMode::miArbCheck:
            EncodeMiArbCheck<GfxFamily>::program(linearStream, std::nullopt);
            return;
        case BlitterConstants::PostBlitMode::miFlush:
            EncodeMiFlushDW<GfxFamily>::programWithWa(linearStream, 0, 0, args);
            return;
        default:
            return;
        }
    }

    if (BlitCommandsHelper<GfxFamily>::miArbCheckWaRequired()) {
        EncodeMiFlushDW<GfxFamily>::programWithWa(linearStream, 0, 0, args);
    }

    EncodeMiArbCheck<GfxFamily>::program(linearStream, std::nullopt);
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimatePostBlitCommandSize() {
    EncodeDummyBlitWaArgs waArgs{};

    if (debugManager.flags.PostBlitCommand.get() != BlitterConstants::PostBlitMode::defaultMode) {
        switch (debugManager.flags.PostBlitCommand.get()) {
        case BlitterConstants::PostBlitMode::miArbCheck:
            return EncodeMiArbCheck<GfxFamily>::getCommandSize();
        case BlitterConstants::PostBlitMode::miFlush:
            return EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
        default:
            return 0;
        }
    }
    size_t size = 0u;
    if (BlitCommandsHelper<GfxFamily>::miArbCheckWaRequired()) {
        size += EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
    }
    size += EncodeMiArbCheck<GfxFamily>::getCommandSize();
    return size;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimateBlitCommandSize(const Vec3<size_t> &copySize, const CsrDependencies &csrDependencies, bool updateTimestampPacket, bool profilingEnabled,
                                                              bool isImage, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed, bool relaxedOrderingEnabled) {
    size_t timestampCmdSize = 0;
    if (updateTimestampPacket) {
        EncodeDummyBlitWaArgs waArgs{true, const_cast<RootDeviceEnvironment *>(&rootDeviceEnvironment)};
        timestampCmdSize += EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
        if (profilingEnabled) {
            timestampCmdSize += getProfilingMmioCmdsSize();
        }
    }

    size_t nBlits = 0u;
    size_t sizePerBlit = 0u;

    if (isImage) {
        nBlits = getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment, isSystemMemoryPoolUsed);
        sizePerBlit = sizeof(typename GfxFamily::XY_BLOCK_COPY_BLT);
    } else {
        nBlits = std::min(getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment, isSystemMemoryPoolUsed),
                          getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, isSystemMemoryPoolUsed));
        sizePerBlit = sizeof(typename GfxFamily::XY_COPY_BLT);
    }

    sizePerBlit += estimatePostBlitCommandSize();
    return TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(csrDependencies, relaxedOrderingEnabled) +
           TimestampPacketHelper::getRequiredCmdStreamSizeForMultiRootDeviceSyncNodesContainer<GfxFamily>(csrDependencies) +
           (sizePerBlit * nBlits) +
           timestampCmdSize +
           estimatePreBlitCommandSize();
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(const BlitPropertiesContainer &blitPropertiesContainer,
                                                               bool profilingEnabled, bool debugPauseEnabled, bool blitterDirectSubmission,
                                                               bool relaxedOrderingEnabled, const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t size = 0;
    EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&rootDeviceEnvironment)};
    for (auto &blitProperties : blitPropertiesContainer) {
        auto updateTimestampPacket = blitProperties.blitSyncProperties.outputTimestampPacket != nullptr;
        auto isImage = blitProperties.isImageOperation();
        size += BlitCommandsHelper<GfxFamily>::estimateBlitCommandSize(blitProperties.copySize, blitProperties.csrDependencies, updateTimestampPacket,
                                                                       profilingEnabled, isImage, rootDeviceEnvironment, blitProperties.isSystemMemoryPoolUsed, relaxedOrderingEnabled);
        if (blitProperties.multiRootDeviceEventSync != nullptr) {
            size += EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
        }

        bool deviceToHostPostSyncFenceRequired = rootDeviceEnvironment.getProductHelper().isDeviceToHostCopySignalingFenceRequired() &&
                                                 (blitProperties.dstAllocation && !blitProperties.dstAllocation->isAllocatedInLocalMemoryPool()) &&
                                                 (blitProperties.srcAllocation && blitProperties.srcAllocation->isAllocatedInLocalMemoryPool());
        if (deviceToHostPostSyncFenceRequired) {
            size += MemorySynchronizationCommands<GfxFamily>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
        }
    }
    waArgs.isWaRequired = true;
    size += BlitCommandsHelper<GfxFamily>::getWaCmdsSize(blitPropertiesContainer);
    size += 2 * MemorySynchronizationCommands<GfxFamily>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
    size += EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
    size += blitterDirectSubmission ? sizeof(typename GfxFamily::MI_BATCH_BUFFER_START) : sizeof(typename GfxFamily::MI_BATCH_BUFFER_END);

    if (debugPauseEnabled) {
        size += BlitCommandsHelper<GfxFamily>::getSizeForDebugPauseCommands(rootDeviceEnvironment);
    }

    size += BlitCommandsHelper<GfxFamily>::getSizeForGlobalSequencerFlush();

    if (relaxedOrderingEnabled) {
        size += 2 * EncodeSetMMIO<GfxFamily>::sizeREG;
    }

    if (debugManager.flags.FlushTlbBeforeCopy.get() == 1) {
        size += EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
    }

    return alignUp(size, MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandDestinationBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice) {
    return blitProperties.dstGpuAddress + blitProperties.dstOffset.x * blitProperties.bytesPerPixel + offset +
           blitProperties.dstOffset.y * blitProperties.dstRowPitch +
           blitProperties.dstOffset.z * blitProperties.dstSlicePitch +
           row * blitProperties.dstRowPitch +
           slice * blitProperties.dstSlicePitch;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandSourceBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice) {
    return blitProperties.srcGpuAddress + blitProperties.srcOffset.x * blitProperties.bytesPerPixel + offset +
           blitProperties.srcOffset.y * blitProperties.srcRowPitch +
           blitProperties.srcOffset.z * blitProperties.srcSlicePitch +
           row * blitProperties.srcRowPitch +
           slice * blitProperties.srcSlicePitch;
}

template <typename GfxFamily>
BlitCommandsResult BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferPerRow(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    uint64_t width = 1;
    uint64_t height = 1;

    PRINT_DEBUG_STRING(debugManager.flags.PrintBlitDispatchDetails.get(), stdout,
                       "\nBlit dispatch with AuxTranslationDirection %u ", static_cast<uint32_t>(blitProperties.auxTranslationDirection));

    dispatchPreBlitCommand(linearStream, rootDeviceEnvironment);
    auto bltCmd = GfxFamily::cmdInitXyCopyBlt;
    const auto maxWidth = getMaxBlitWidth(rootDeviceEnvironment);
    const auto maxHeight = getMaxBlitHeight(rootDeviceEnvironment, blitProperties.isSystemMemoryPoolUsed);

    appendColorDepth(blitProperties, bltCmd);

    const bool useAdditionalBlitProperties = rootDeviceEnvironment.getHelper<ProductHelper>().useAdditionalBlitProperties();

    BlitCommandsResult result{};
    bool firstCommand = true;
    for (uint64_t slice = 0; slice < blitProperties.copySize.z; slice++) {
        for (uint64_t row = 0; row < blitProperties.copySize.y; row++) {
            uint64_t offset = 0;
            uint64_t sizeToBlit = blitProperties.copySize.x;
            bool lastIteration = (slice == blitProperties.copySize.z - 1) && (row == blitProperties.copySize.y - 1);
            while (sizeToBlit != 0) {
                auto tmpCmd = bltCmd;
                if (sizeToBlit > maxWidth) {
                    // dispatch 2D blit: maxBlitWidth x (1 .. maxBlitHeight)
                    width = maxWidth;
                    height = std::min((sizeToBlit / width), maxHeight);
                } else {
                    // dispatch 1D blt: (1 .. maxBlitWidth) x 1
                    width = sizeToBlit;
                    height = 1;
                }
                auto blitSize = width * height;
                bool lastCommand = lastIteration && (sizeToBlit - blitSize == 0);

                tmpCmd.setDestinationX2CoordinateRight(static_cast<uint32_t>(width));
                tmpCmd.setDestinationY2CoordinateBottom(static_cast<uint32_t>(height));
                tmpCmd.setDestinationPitch(static_cast<uint32_t>(width));
                tmpCmd.setSourcePitch(static_cast<uint32_t>(width));

                auto dstAddr = calculateBlitCommandDestinationBaseAddress(blitProperties, offset, row, slice);
                auto srcAddr = calculateBlitCommandSourceBaseAddress(blitProperties, offset, row, slice);

                PRINT_DEBUG_STRING(debugManager.flags.PrintBlitDispatchDetails.get(), stdout,
                                   "\nBlit command. width: %u, height: %u, srcAddr: %#llx, dstAddr: %#llx ", width, height, srcAddr, dstAddr);

                tmpCmd.setDestinationBaseAddress(dstAddr);
                tmpCmd.setSourceBaseAddress(srcAddr);
                if (useAdditionalBlitProperties && (firstCommand || lastCommand)) {
                    applyAdditionalBlitProperties(blitProperties, tmpCmd, rootDeviceEnvironment, lastCommand);
                    firstCommand = false;
                }

                appendBlitCommandsForBuffer(blitProperties, tmpCmd, rootDeviceEnvironment);

                auto bltStream = linearStream.getSpaceForCmd<typename GfxFamily::XY_COPY_BLT>();
                *bltStream = tmpCmd;
                if (lastCommand) {
                    result.lastBlitCommand = bltStream;
                }
                dispatchPostBlitCommand(linearStream, rootDeviceEnvironment);

                sizeToBlit -= blitSize;
                offset += blitSize;
            }
        }
    }
    return result;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitMemSetCommand(const BlitProperties &blitProperties, void *blitCmd) {}

template <typename GfxFamily>
BlitCommandsResult BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
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
    const bool useAdditionalBlitProperties = rootDeviceEnvironment.getHelper<ProductHelper>().useAdditionalBlitProperties();

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

        if (useAdditionalBlitProperties && (firstCommand || lastCommand)) {
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

template <typename GfxFamily>
BlitCommandsResult BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForImageRegion(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {

    auto srcSlicePitch = static_cast<uint32_t>(blitProperties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(blitProperties.dstSlicePitch);

    UNRECOVERABLE_IF(blitProperties.copySize.x > BlitterConstants::maxBlitWidth || blitProperties.copySize.y > BlitterConstants::maxBlitHeight);
    auto bltCmd = GfxFamily::cmdInitXyBlockCopyBlt;

    bltCmd.setSourceBaseAddress(blitProperties.srcGpuAddress);
    bltCmd.setDestinationBaseAddress(blitProperties.dstGpuAddress);

    bltCmd.setDestinationX1CoordinateLeft(static_cast<uint32_t>(blitProperties.dstOffset.x));
    bltCmd.setDestinationY1CoordinateTop(static_cast<uint32_t>(blitProperties.dstOffset.y));
    bltCmd.setDestinationX2CoordinateRight(static_cast<uint32_t>(blitProperties.dstOffset.x + blitProperties.copySize.x));
    bltCmd.setDestinationY2CoordinateBottom(static_cast<uint32_t>(blitProperties.dstOffset.y + blitProperties.copySize.y));

    bltCmd.setSourceX1CoordinateLeft(static_cast<uint32_t>(blitProperties.srcOffset.x));
    bltCmd.setSourceY1CoordinateTop(static_cast<uint32_t>(blitProperties.srcOffset.y));

    appendBlitCommandsBlockCopy(blitProperties, bltCmd, rootDeviceEnvironment);
    appendBlitCommandsForImages(blitProperties, bltCmd, rootDeviceEnvironment, srcSlicePitch, dstSlicePitch);
    appendColorDepth(blitProperties, bltCmd);
    appendSurfaceType(blitProperties, bltCmd);

    dispatchPreBlitCommand(linearStream, rootDeviceEnvironment);
    const bool useAdditionalBlitProperties = rootDeviceEnvironment.getHelper<ProductHelper>().useAdditionalBlitProperties();

    BlitCommandsResult result{};
    for (uint32_t i = 0; i < blitProperties.copySize.z; i++) {
        auto tmpCmd = bltCmd;
        appendSliceOffsets(blitProperties, tmpCmd, i, rootDeviceEnvironment, srcSlicePitch, dstSlicePitch);

        if (debugManager.flags.PrintImageBlitBlockCopyCmdDetails.get()) {
            printImageBlitBlockCopyCommand(tmpCmd, i);
        }
        bool lastCommand = (i == (blitProperties.copySize.z - 1));
        if (useAdditionalBlitProperties && (i == 0 || lastCommand)) {
            applyAdditionalBlitProperties(blitProperties, tmpCmd, rootDeviceEnvironment, lastCommand);
        }
        auto cmd = linearStream.getSpaceForCmd<typename GfxFamily::XY_BLOCK_COPY_BLT>();
        *cmd = tmpCmd;
        if (lastCommand) {
            result.lastBlitCommand = cmd;
        }
        dispatchPostBlitCommand(linearStream, rootDeviceEnvironment);
    }
    return result;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(LinearStream &commandStream, uint64_t debugPauseStateGPUAddress,
                                                               DebugPauseState confirmationTrigger, DebugPauseState waitCondition,
                                                               RootDeviceEnvironment &rootDeviceEnvironment) {
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    NEO::EncodeDummyBlitWaArgs waArgs{false, &rootDeviceEnvironment};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, debugPauseStateGPUAddress, static_cast<uint32_t>(confirmationTrigger),
                                              args);

    EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(commandStream, debugPauseStateGPUAddress, static_cast<uint32_t>(waitCondition), COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getSizeForDebugPauseCommands(const RootDeviceEnvironment &rootDeviceEnvironment) {
    EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&rootDeviceEnvironment)};
    return (EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs) + EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait()) * 2;
}

template <typename GfxFamily>
uint32_t BlitCommandsHelper<GfxFamily>::getAvailableBytesPerPixel(size_t copySize, uint32_t srcOrigin, uint32_t dstOrigin, size_t srcSize, size_t dstSize) {
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
void BlitCommandsHelper<GfxFamily>::dispatchBlitCommands(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    if (blitProperties.isImageOperation()) {
        dispatchBlitCommandsForImageRegion(blitProperties, linearStream, rootDeviceEnvironment);
    } else {
        bool preferCopyBufferRegion = isCopyRegionPreferred(blitProperties.copySize, rootDeviceEnvironment, blitProperties.isSystemMemoryPoolUsed);
        preferCopyBufferRegion ? dispatchBlitCommandsForBufferRegion(blitProperties, linearStream, rootDeviceEnvironment)
                               : dispatchBlitCommandsForBufferPerRow(blitProperties, linearStream, rootDeviceEnvironment);
    }
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandSourceBaseAddressCopyRegion(const BlitProperties &blitProperties, size_t slice) {
    return blitProperties.srcGpuAddress + blitProperties.srcOffset.x * blitProperties.bytesPerPixel +
           (blitProperties.srcOffset.y * blitProperties.srcRowPitch) +
           (blitProperties.srcSlicePitch * (slice + blitProperties.srcOffset.z));
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::calculateBlitCommandDestinationBaseAddressCopyRegion(const BlitProperties &blitProperties, size_t slice) {
    return blitProperties.dstGpuAddress + blitProperties.dstOffset.x * blitProperties.bytesPerPixel +
           (blitProperties.dstOffset.y * blitProperties.dstRowPitch) +
           (blitProperties.dstSlicePitch * (slice + blitProperties.dstOffset.z));
}

template <typename GfxFamily>
template <typename T>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForBuffer(const BlitProperties &blitProperties, T &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    appendBlitCommandsBlockCopy(blitProperties, blitCmd, rootDeviceEnvironment);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsMemCopy(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd,
                                                              const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <typename GfxFamily>
BlitCommandsResult BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferRegion(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {

    const auto maxWidthToCopy = getMaxBlitWidth(rootDeviceEnvironment);
    const auto maxHeightToCopy = getMaxBlitHeight(rootDeviceEnvironment, blitProperties.isSystemMemoryPoolUsed);

    dispatchPreBlitCommand(linearStream, rootDeviceEnvironment);

    auto bltCmd = GfxFamily::cmdInitXyCopyBlt;
    bltCmd.setSourcePitch(static_cast<uint32_t>(blitProperties.srcRowPitch));
    bltCmd.setDestinationPitch(static_cast<uint32_t>(blitProperties.dstRowPitch));
    appendColorDepth(blitProperties, bltCmd);

    const bool useAdditionalBlitProperties = rootDeviceEnvironment.getHelper<ProductHelper>().useAdditionalBlitProperties();

    BlitCommandsResult result{};
    bool firstCommand = true;
    for (size_t slice = 0u; slice < blitProperties.copySize.z; ++slice) {
        auto srcAddress = calculateBlitCommandSourceBaseAddressCopyRegion(blitProperties, slice);
        auto dstAddress = calculateBlitCommandDestinationBaseAddressCopyRegion(blitProperties, slice);
        auto heightToCopy = blitProperties.copySize.y;

        while (heightToCopy > 0) {

            auto height = static_cast<uint32_t>(std::min(heightToCopy, static_cast<size_t>(maxHeightToCopy)));
            auto widthToCopy = blitProperties.copySize.x;

            bool lastIteration = (slice == blitProperties.copySize.z - 1) && ((heightToCopy - height) == 0);

            while (widthToCopy > 0) {
                auto tmpCmd = bltCmd;
                auto width = static_cast<uint32_t>(std::min(widthToCopy, static_cast<size_t>(maxWidthToCopy)));

                bool lastCommand = lastIteration && ((widthToCopy - width) == 0);

                tmpCmd.setSourceBaseAddress(srcAddress);
                tmpCmd.setDestinationBaseAddress(dstAddress);
                tmpCmd.setDestinationX2CoordinateRight(width);
                tmpCmd.setDestinationY2CoordinateBottom(height);

                appendBlitCommandsForBuffer(blitProperties, tmpCmd, rootDeviceEnvironment);

                if (useAdditionalBlitProperties && (firstCommand || lastCommand)) {
                    applyAdditionalBlitProperties(blitProperties, tmpCmd, rootDeviceEnvironment, lastCommand);
                    firstCommand = false;
                }
                auto cmd = linearStream.getSpaceForCmd<typename GfxFamily::XY_COPY_BLT>();
                *cmd = tmpCmd;
                if (lastCommand) {
                    result.lastBlitCommand = cmd;
                }
                dispatchPostBlitCommand(linearStream, rootDeviceEnvironment);

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
    return result;
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::isCopyRegionPreferred(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    bool preferCopyRegion = getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment, isSystemMemoryPoolUsed) < getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment, isSystemMemoryPoolUsed);
    return preferCopyRegion;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForCopyRegion(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    auto maxWidthToCopy = getMaxBlitWidth(rootDeviceEnvironment);
    auto maxHeightToCopy = getMaxBlitHeight(rootDeviceEnvironment, isSystemMemoryPoolUsed);
    auto xBlits = static_cast<size_t>(std::ceil(copySize.x / static_cast<double>(maxWidthToCopy)));
    auto yBlits = static_cast<size_t>(std::ceil(copySize.y / static_cast<double>(maxHeightToCopy)));
    auto zBlits = static_cast<size_t>(copySize.z);
    auto nBlits = xBlits * yBlits * zBlits;

    return nBlits;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForCopyPerRow(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    size_t xBlits = 0u;
    uint64_t width = 1;
    uint64_t height = 1;
    uint64_t sizeToBlit = copySize.x;
    const auto maxWidth = getMaxBlitWidth(rootDeviceEnvironment);
    const auto maxHeight = getMaxBlitHeight(rootDeviceEnvironment, isSystemMemoryPoolUsed);

    while (sizeToBlit != 0) {
        if (sizeToBlit > maxWidth) {
            // dispatch 2D blit: maxBlitWidth x (1 .. maxBlitHeight)
            width = maxWidth;
            height = std::min((sizeToBlit / width), maxHeight);

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

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    auto maxWidthToFill = getMaxBlitWidth(rootDeviceEnvironment);
    auto maxHeightToFill = getMaxBlitHeight(rootDeviceEnvironment, isSystemMemoryPoolUsed);
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
size_t BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForColorFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    if (patternSize == 1) {
        return NEO::BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForByteFill(copySize, patternSize, rootDeviceEnvironment, isSystemMemoryPoolUsed);
    } else {
        return NEO::BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForFill(copySize, patternSize, rootDeviceEnvironment, isSystemMemoryPoolUsed);
    }
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::preBlitCommandWARequired() {
    return false;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendExtraMemoryProperties(typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendExtraMemoryProperties(typename GfxFamily::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::encodeProfilingStartMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextStartGpuAddress = TimestampPacketHelper::getContextStartGpuAddress(timestampPacketNode);
    auto timestampGlobalStartAddress = TimestampPacketHelper::getGlobalStartGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<GfxFamily>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextStartGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<GfxFamily>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalStartAddress, false, nullptr, true);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::encodeProfilingEndMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextEndGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(timestampPacketNode);
    auto timestampGlobalEndAddress = TimestampPacketHelper::getGlobalEndGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<GfxFamily>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextEndGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<GfxFamily>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalEndAddress, false, nullptr, true);
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getProfilingMmioCmdsSize() {
    return 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBaseAddressOffset(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const bool isSource) {}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::encodeWa(LinearStream &cmdStream, const BlitProperties &blitProperties, uint32_t &latestSentBcsWaValue) {
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getWaCmdsSize(const BlitPropertiesContainer &blitPropertiesContainer) {
    return 0;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::adjustControlSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitFillCommand(const BlitProperties &blitProperties, typename GfxFamily::XY_COLOR_BLT &blitCmd) {}

template <typename GfxFamily>
BlitCommandsResult BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    if (blitProperties.fillPatternSize == 1) {
        return NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryByteFill(blitProperties, linearStream, rootDeviceEnvironment);
    } else {
        return NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill(blitProperties, linearStream, rootDeviceEnvironment);
    }
}

} // namespace NEO
