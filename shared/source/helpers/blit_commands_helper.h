/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/blit_properties_container.h"
#include "shared/source/helpers/vec.h"

#include <cstdint>

namespace NEO {

class CsrDependencies;
class GraphicsAllocation;
class LinearStream;
class TagNodeBase;
enum class DebugPauseState : uint32_t;
struct HardwareInfo;
struct RootDeviceEnvironment;
class ProductHelper;
struct EncodeDummyBlitWaArgs;

struct BlitCommandsResult {
    void *lastBlitCommand = nullptr;
};

template <typename GfxFamily>
struct BlitCommandsHelper {
    using COLOR_DEPTH = typename GfxFamily::XY_COLOR_BLT::COLOR_DEPTH;
    static uint64_t getMaxBlitWidth(const RootDeviceEnvironment &rootDeviceEnvironment);
    static uint64_t getMaxBlitWidthOverride(const RootDeviceEnvironment &rootDeviceEnvironment);
    static uint64_t getMaxBlitHeight(const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static uint64_t getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static uint64_t getMaxBlitSetWidth(const RootDeviceEnvironment &rootDeviceEnvironment);
    static uint64_t getMaxBlitSetHeight(const RootDeviceEnvironment &rootDeviceEnvironment);
    static void dispatchPreBlitCommand(LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t estimatePreBlitCommandSize();
    static void dispatchPostBlitCommand(LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t estimatePostBlitCommandSize();
    static size_t estimateBlitCommandSize(const Vec3<size_t> &copySize, const CsrDependencies &csrDependencies, bool updateTimestampPacket,
                                          bool profilingEnabled, bool isImage, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed, bool relaxedOrderingEnabled);
    static size_t estimateBlitCommandsSize(const BlitPropertiesContainer &blitPropertiesContainer, bool profilingEnabled,
                                           bool debugPauseEnabled, bool blitterDirectSubmission, bool relaxedOrderingEnabled, const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getNumberOfBlitsForCopyRegion(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static size_t getNumberOfBlitsForCopyPerRow(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static size_t getNumberOfBlitsForColorFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static size_t getNumberOfBlitsForByteFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static size_t getNumberOfBlitsForFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static uint64_t calculateBlitCommandDestinationBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice);
    static uint64_t calculateBlitCommandSourceBaseAddress(const BlitProperties &blitProperties, uint64_t offset, uint64_t row, uint64_t slice);
    static uint64_t calculateBlitCommandDestinationBaseAddressCopyRegion(const BlitProperties &blitProperties, size_t slice);
    static uint64_t calculateBlitCommandSourceBaseAddressCopyRegion(const BlitProperties &blitProperties, size_t slice);
    static void dispatchBlitCommands(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static BlitCommandsResult dispatchBlitCommandsForBufferRegion(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static BlitCommandsResult dispatchBlitCommandsForBufferPerRow(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static BlitCommandsResult dispatchBlitCommandsForImageRegion(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static BlitCommandsResult dispatchBlitMemoryColorFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static BlitCommandsResult dispatchBlitMemoryByteFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static BlitCommandsResult dispatchBlitMemoryFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment);
    static void dispatchDummyBlit(LinearStream &linearStream, EncodeDummyBlitWaArgs &waArgs);
    static size_t getDummyBlitSize(const EncodeDummyBlitWaArgs &waArgs);
    static bool isDummyBlitWaNeeded(const EncodeDummyBlitWaArgs &waArgs);

    template <typename T>
    static void appendBlitCommandsForBuffer(const BlitProperties &blitProperties, T &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void appendBlitCommandsMemCopy(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void appendBlitCommandsForImages(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch);
    static void adjustControlSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd);
    static void appendExtraMemoryProperties(typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void appendExtraMemoryProperties(typename GfxFamily::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);
    template <typename T = typename GfxFamily::XY_BLOCK_COPY_BLT>
    static void appendColorDepth(const BlitProperties &blitProperties, T &blitCmd);
    static void appendBlitMemoryOptionsForFillBuffer(NEO::GraphicsAllocation *dstAlloc, typename GfxFamily::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void appendBlitFillCommand(const BlitProperties &blitProperties, typename GfxFamily::XY_COLOR_BLT &blitCmd);
    static void appendBlitMemSetCompressionFormat(void *blitCmd, NEO::GraphicsAllocation *dstAlloc, uint32_t compressionFormat);
    static void appendBlitMemSetCommand(const BlitProperties &blitProperties, void *blitCmd);
    static void appendSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd);
    static void appendTilingEnable(typename GfxFamily::XY_COLOR_BLT &blitCmd);
    static void appendTilingType(const GMM_TILE_TYPE srcTilingType, const GMM_TILE_TYPE dstTilingType, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd);
    static void appendSliceOffsets(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, uint32_t sliceIndex, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t srcSlicePitch, uint32_t dstSlicePitch);
    static void appendBaseAddressOffset(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const bool isSource);
    static void getBlitAllocationProperties(const GraphicsAllocation &allocation, uint32_t &pitch, uint32_t &qPitch, GMM_TILE_TYPE &tileType,
                                            uint32_t &mipTailLod, uint32_t &compressionDetails,
                                            const RootDeviceEnvironment &rootDeviceEnvironment, GMM_YUV_PLANE_ENUM plane);
    static void dispatchDebugPauseCommands(LinearStream &commandStream, uint64_t debugPauseStateGPUAddress, DebugPauseState confirmationTrigger,
                                           DebugPauseState waitCondition, RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeForDebugPauseCommands(const RootDeviceEnvironment &rootDeviceEnvironment);
    static uint32_t getAvailableBytesPerPixel(size_t copySize, uint32_t srcOrigin, uint32_t dstOrigin, size_t srcSize, size_t dstSize);
    static bool isCopyRegionPreferred(const Vec3<size_t> &copySize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed);
    static void programGlobalSequencerFlush(LinearStream &commandStream);
    static size_t getSizeForGlobalSequencerFlush();
    static bool miArbCheckWaRequired();
    static bool preBlitCommandWARequired();
    static void appendClearColor(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd);
    static void printImageBlitBlockCopyCommand(const typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const uint32_t sliceIndex);

    static void encodeProfilingStartMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode);
    static void encodeProfilingEndMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode);
    static size_t getProfilingMmioCmdsSize();

    static void encodeWa(LinearStream &cmdStream, const BlitProperties &blitProperties, uint32_t &latestSentBcsWaValue);
    static size_t getWaCmdsSize(const BlitPropertiesContainer &blitPropertiesContainer);

    template <typename CommandType>
    static void applyAdditionalBlitProperties(const BlitProperties &blitProperties, CommandType &cmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last);
};
} // namespace NEO
