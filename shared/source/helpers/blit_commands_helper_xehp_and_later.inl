/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_commands_helper_base.inl"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitWidthOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (productHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::cpuAccessAllowed) {
        return 1024;
    }
    return 0;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (productHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::cpuAccessAllowed) {
        return 1024;
    }
    return 0;
}

template <typename GfxFamily>
void setCompressionParamsForFillOperation(typename GfxFamily::XY_COLOR_BLT &xyColorBlt) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    xyColorBlt.setDestinationCompressionEnable(XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_ENABLE);
    xyColorBlt.setDestinationAuxiliarysurfacemode(XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitMemoryOptionsForFillBuffer(NEO::GraphicsAllocation *dstAlloc, typename GfxFamily::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    uint32_t compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);
    if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        compressionFormat = debugManager.flags.ForceBufferCompressionFormat.get();
    }

    if (dstAlloc->isCompressionEnabled()) {
        setCompressionParamsForFillOperation<GfxFamily>(blitCmd);
        blitCmd.setDestinationCompressionFormat(compressionFormat);
    }

    blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);

    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getUncachedMOCS();
    if (debugManager.flags.OverrideBlitterMocs.get() != -1) {
        mocs = static_cast<uint32_t>(debugManager.flags.OverrideBlitterMocs.get());
    }

    blitCmd.setDestinationMOCS(mocs);

    if (debugManager.flags.OverrideBlitterTargetMemory.get() != -1) {
        if (debugManager.flags.OverrideBlitterTargetMemory.get() == 0u) {
            blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
        } else if (debugManager.flags.OverrideBlitterTargetMemory.get() == 1u) {
            blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;

    if (blitProperties.srcAllocation->getDefaultGmm()) {
        auto resInfo = blitProperties.srcAllocation->getDefaultGmm()->gmmResourceInfo.get();
        auto resourceType = resInfo->getResourceType();
        auto isArray = resInfo->getArraySize() > 1;
        auto isTiled = resInfo->getResourceFlags()->Info.Tile4 || resInfo->getResourceFlags()->Info.Tile64;

        if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_1D) {
            if (isArray || isTiled) {
                blitCmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
            } else {
                blitCmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
            }

        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_2D) {
            blitCmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
            blitCmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D);
        }
    }

    if (blitProperties.dstAllocation->getDefaultGmm()) {
        auto resInfo = blitProperties.dstAllocation->getDefaultGmm()->gmmResourceInfo.get();
        auto resourceType = resInfo->getResourceType();
        auto isArray = resInfo->getArraySize() > 1;
        auto isTiled = resInfo->getResourceFlags()->Info.Tile4 || resInfo->getResourceFlags()->Info.Tile64;

        if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_1D) {
            if (isArray || isTiled) {
                blitCmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
            } else {
                blitCmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
            }
        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_2D) {
            blitCmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
            blitCmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D);
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingType(const GMM_TILE_TYPE srcTilingType, const GMM_TILE_TYPE dstTilingType, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    UNRECOVERABLE_IF((srcTilingType != dstTilingType) && blitCmd.getSpecialModeOfOperation() == XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE);
    if (srcTilingType == GMM_TILED_4) {
        blitCmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE4);
    } else if (srcTilingType == GMM_TILED_64) {
        blitCmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE64);
    } else {
        blitCmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING::TILING_LINEAR);
    }
    if (dstTilingType == GMM_TILED_4) {
        blitCmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE4);
    } else if (dstTilingType == GMM_TILED_64) {
        blitCmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE64);
    } else {
        blitCmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING::TILING_LINEAR);
    }
}

template <typename GfxFamily>
template <typename T>
void BlitCommandsHelper<GfxFamily>::appendColorDepth(const BlitProperties &blitProperties, T &blitCmd) {
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    if constexpr (std::is_same_v<XY_BLOCK_COPY_BLT, T>) {
        switch (blitProperties.bytesPerPixel) {
        default:
            UNRECOVERABLE_IF(true);
            break;
        case 1:
            blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
            break;
        case 2:
            blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
            break;
        case 4:
            blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
            break;
        case 8:
            blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
            break;
        case 16:
            blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
            break;
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::getBlitAllocationProperties(const GraphicsAllocation &allocation, uint32_t &pitch, uint32_t &qPitch,
                                                                GMM_TILE_TYPE &tileType, uint32_t &mipTailLod, uint32_t &compressionDetails,
                                                                const RootDeviceEnvironment &rootDeviceEnvironment, GMM_YUV_PLANE_ENUM plane) {
    if (allocation.getDefaultGmm()) {
        auto gmmResourceInfo = allocation.getDefaultGmm()->gmmResourceInfo.get();
        mipTailLod = gmmResourceInfo->getMipTailStartLODSurfaceState();
        auto resInfo = gmmResourceInfo->getResourceFlags()->Info;
        if (resInfo.Tile4) {
            tileType = GMM_TILED_4;
        } else if (resInfo.Tile64) {
            tileType = GMM_TILED_64;
        }

        if (!resInfo.Linear) {
            qPitch = gmmResourceInfo->getQPitch() ? static_cast<uint32_t>(gmmResourceInfo->getQPitch()) : qPitch;
            pitch = gmmResourceInfo->getRenderPitch() ? static_cast<uint32_t>(gmmResourceInfo->getRenderPitch()) : pitch;
        }

        auto gmmClientContext = rootDeviceEnvironment.getGmmClientContext();
        if (resInfo.MediaCompressed) {
            compressionDetails = gmmClientContext->getMediaSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
            EncodeWA<GfxFamily>::adjustCompressionFormatForPlanarImage(compressionDetails, plane);
        } else if (resInfo.RenderCompressed) {
            compressionDetails = gmmClientContext->getSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForImages(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch) {
    auto srcTileType = GMM_NOT_TILED;
    auto dstTileType = GMM_NOT_TILED;
    auto srcAllocation = blitProperties.srcAllocation;
    auto dstAllocation = blitProperties.dstAllocation;
    auto srcRowPitch = static_cast<uint32_t>(blitProperties.srcRowPitch);
    auto dstRowPitch = static_cast<uint32_t>(blitProperties.dstRowPitch);
    auto srcQPitch = static_cast<uint32_t>(blitProperties.srcSize.y);
    auto dstQPitch = static_cast<uint32_t>(blitProperties.dstSize.y);
    auto srcMipTailLod = 0u;
    auto dstMipTailLod = 0u;
    auto srcCompressionFormat = blitCmd.getSourceCompressionFormat();
    auto dstCompressionFormat = blitCmd.getDestinationCompressionFormat();

    getBlitAllocationProperties(*srcAllocation, srcRowPitch, srcQPitch, srcTileType, srcMipTailLod, srcCompressionFormat,
                                rootDeviceEnvironment, blitProperties.srcPlane);
    getBlitAllocationProperties(*dstAllocation, dstRowPitch, dstQPitch, dstTileType, dstMipTailLod, dstCompressionFormat,
                                rootDeviceEnvironment, blitProperties.dstPlane);

    srcSlicePitch = std::max(srcSlicePitch, srcRowPitch * srcQPitch);
    dstSlicePitch = std::max(dstSlicePitch, dstRowPitch * dstQPitch);

    blitCmd.setSourcePitch(srcTileType == GMM_NOT_TILED ? srcRowPitch : srcRowPitch / 4);
    blitCmd.setDestinationPitch(dstTileType == GMM_NOT_TILED ? dstRowPitch : dstRowPitch / 4);
    blitCmd.setSourceSurfaceQpitch(srcQPitch / 4);
    blitCmd.setDestinationSurfaceQpitch(dstQPitch / 4);
    blitCmd.setSourceMipTailStartLOD(srcMipTailLod);
    blitCmd.setDestinationMipTailStartLOD(dstMipTailLod);
    blitCmd.setSourceSurfaceWidth(static_cast<uint32_t>(blitProperties.srcSize.x));
    blitCmd.setSourceSurfaceHeight(static_cast<uint32_t>(blitProperties.srcSize.y));
    blitCmd.setSourceSurfaceDepth(static_cast<uint32_t>(blitProperties.srcSize.z));
    blitCmd.setDestinationSurfaceWidth(static_cast<uint32_t>(blitProperties.dstSize.x));
    blitCmd.setDestinationSurfaceHeight(static_cast<uint32_t>(blitProperties.dstSize.y));
    blitCmd.setDestinationSurfaceDepth(static_cast<uint32_t>(blitProperties.dstSize.z));
    blitCmd.setSourceCompressionFormat(srcCompressionFormat);
    blitCmd.setDestinationCompressionFormat(dstCompressionFormat);

    appendTilingType(srcTileType, dstTileType, blitCmd);
    appendClearColor(blitProperties, blitCmd);
    adjustControlSurfaceType(blitProperties, blitCmd);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSliceOffsets(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, uint32_t sliceIndex, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t srcSlicePitch, uint32_t dstSlicePitch) {
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    auto srcAddress = blitProperties.srcGpuAddress;
    auto dstAddress = blitProperties.dstGpuAddress;

    if (blitCmd.getSourceTiling() == XY_BLOCK_COPY_BLT::TILING::TILING_LINEAR) {
        blitCmd.setSourceBaseAddress(ptrOffset(srcAddress, srcSlicePitch * (sliceIndex + blitProperties.srcOffset.z)));
    } else {
        blitCmd.setSourceArrayIndex((sliceIndex + static_cast<uint32_t>(blitProperties.srcOffset.z)) + 1);
    }
    if (blitCmd.getDestinationTiling() == XY_BLOCK_COPY_BLT::TILING::TILING_LINEAR) {
        blitCmd.setDestinationBaseAddress(ptrOffset(dstAddress, dstSlicePitch * (sliceIndex + blitProperties.dstOffset.z)));
    } else {
        blitCmd.setDestinationArrayIndex((sliceIndex + static_cast<uint32_t>(blitProperties.dstOffset.z)) + 1);
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingEnable(typename GfxFamily::XY_COLOR_BLT &blitCmd) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    blitCmd.setDestinationSurfaceType(XY_COLOR_BLT::DESTINATION_SURFACE_TYPE::DESTINATION_SURFACE_TYPE_SURFTYPE_2D);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::programGlobalSequencerFlush(LinearStream &commandStream) {
    if (debugManager.flags.GlobalSequencerFlushOnCopyEngine.get() != 0) {
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
        constexpr uint32_t globalInvalidationRegister = 0xB404u;
        LriHelper<GfxFamily>::program(&commandStream, globalInvalidationRegister, 1u, false, true);
        EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(commandStream,
                                                              globalInvalidationRegister,
                                                              0u,
                                                              COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                              true, false, false, false, nullptr);
    }
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getSizeForGlobalSequencerFlush() {
    if (debugManager.flags.GlobalSequencerFlushOnCopyEngine.get() != 0) {
        return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM) + NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
    }
    return 0u;
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::miArbCheckWaRequired() {
    return true;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendClearColor(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::printImageBlitBlockCopyCommand(const typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const uint32_t sliceIndex) {
    printf("Slice index: %u\n", sliceIndex);
    printf("ColorDepth: %u\n", blitCmd.getColorDepth());
    printf("SourcePitch: %u\n", blitCmd.getSourcePitch());
    printf("SourceTiling: %u\n", blitCmd.getSourceTiling());
    printf("SourceX1Coordinate_Left: %u\n", blitCmd.getSourceX1CoordinateLeft());
    printf("SourceY1Coordinate_Top: %u\n", blitCmd.getSourceY1CoordinateTop());
    printf("SourceBaseAddress: %" PRIx64 "\n", blitCmd.getSourceBaseAddress());
    printf("SourceXOffset: %u\n", blitCmd.getSourceXOffset());
    printf("SourceYOffset: %u\n", blitCmd.getSourceYOffset());
    printf("SourceTargetMemory: %u\n", blitCmd.getSourceTargetMemory());
    printf("SourceCompressionFormat: %u\n", blitCmd.getSourceCompressionFormat());
    printf("SourceSurfaceHeight: %u\n", blitCmd.getSourceSurfaceHeight());
    printf("SourceSurfaceWidth: %u\n", blitCmd.getSourceSurfaceWidth());
    printf("SourceSurfaceType: %u\n", blitCmd.getSourceSurfaceType());
    printf("SourceSurfaceQpitch: %u\n", blitCmd.getSourceSurfaceQpitch());
    printf("SourceSurfaceDepth: %u\n", blitCmd.getSourceSurfaceDepth());
    printf("SourceHorizontalAlign: %u\n", blitCmd.getSourceHorizontalAlign());
    printf("SourceVerticalAlign: %u\n", blitCmd.getSourceVerticalAlign());
    printf("SourceArrayIndex: %u\n", blitCmd.getSourceArrayIndex());
    printf("DestinationPitch: %u\n", blitCmd.getDestinationPitch());
    printf("DestinationTiling: %u\n", blitCmd.getDestinationTiling());
    printf("DestinationX1Coordinate_Left: %u\n", blitCmd.getDestinationX1CoordinateLeft());
    printf("DestinationY1Coordinate_Top: %u\n", blitCmd.getDestinationY1CoordinateTop());
    printf("DestinationX2Coordinate_Right: %u\n", blitCmd.getDestinationX2CoordinateRight());
    printf("DestinationY2Coordinate_Bottom: %u\n", blitCmd.getDestinationY2CoordinateBottom());
    printf("DestinationBaseAddress: %" PRIx64 "\n", blitCmd.getDestinationBaseAddress());
    printf("DestinationXOffset: %u\n", blitCmd.getDestinationXOffset());
    printf("DestinationYOffset: %u\n", blitCmd.getDestinationYOffset());
    printf("DestinationTargetMemory: %u\n", blitCmd.getDestinationTargetMemory());
    printf("DestinationCompressionFormat: %u\n", blitCmd.getDestinationCompressionFormat());
    printf("DestinationSurfaceHeight: %u\n", blitCmd.getDestinationSurfaceHeight());
    printf("DestinationSurfaceWidth: %u\n", blitCmd.getDestinationSurfaceWidth());
    printf("DestinationSurfaceType: %u\n", blitCmd.getDestinationSurfaceType());
    printf("DestinationSurfaceQpitch: %u\n", blitCmd.getDestinationSurfaceQpitch());
    printf("DestinationSurfaceDepth: %u\n", blitCmd.getDestinationSurfaceDepth());
    printf("DestinationHorizontalAlign: %u\n", blitCmd.getDestinationHorizontalAlign());
    printf("DestinationVerticalAlign: %u\n", blitCmd.getDestinationVerticalAlign());
    printf("DestinationArrayIndex: %u\n\n", blitCmd.getDestinationArrayIndex());
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::isDummyBlitWaNeeded(const EncodeDummyBlitWaArgs &waArgs) {
    if (waArgs.isWaRequired) {
        UNRECOVERABLE_IF(!waArgs.rootDeviceEnvironment);
        if (debugManager.flags.ForceDummyBlitWa.get() != -1) {
            return debugManager.flags.ForceDummyBlitWa.get();
        }
        auto releaseHelper = waArgs.rootDeviceEnvironment->getReleaseHelper();
        UNRECOVERABLE_IF(!releaseHelper);
        return releaseHelper->isDummyBlitWaRequired();
    }
    return false;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchDummyBlit(LinearStream &linearStream, EncodeDummyBlitWaArgs &waArgs) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    if (BlitCommandsHelper<GfxFamily>::isDummyBlitWaNeeded(waArgs)) {
        auto blitCmd = GfxFamily::cmdInitXyColorBlt;
        auto &rootDeviceEnvironment = waArgs.rootDeviceEnvironment;

        rootDeviceEnvironment->initDummyAllocation();
        auto dummyAllocation = rootDeviceEnvironment->getDummyAllocation();
        blitCmd.setDestinationBaseAddress(dummyAllocation->getGpuAddress());
        blitCmd.setColorDepth(COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
        blitCmd.setDestinationX2CoordinateRight(1u);
        blitCmd.setDestinationY2CoordinateBottom(4u);
        blitCmd.setDestinationPitch(static_cast<uint32_t>(MemoryConstants::pageSize));

        appendTilingEnable(blitCmd);
        appendBlitMemoryOptionsForFillBuffer(dummyAllocation, blitCmd, *rootDeviceEnvironment);

        BlitProperties blitProperties = {};
        blitProperties.dstAllocation = dummyAllocation;

        appendBlitFillCommand(blitProperties, blitCmd);

        auto cmd = linearStream.getSpaceForCmd<XY_COLOR_BLT>();
        *cmd = blitCmd;
    }
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getDummyBlitSize(const EncodeDummyBlitWaArgs &waArgs) {
    if (BlitCommandsHelper<GfxFamily>::isDummyBlitWaNeeded(waArgs)) {
        return sizeof(typename GfxFamily::XY_COLOR_BLT);
    }
    return 0u;
}
} // namespace NEO
