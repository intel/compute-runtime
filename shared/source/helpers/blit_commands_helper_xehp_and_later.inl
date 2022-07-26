/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_commands_helper_base.inl"

#include <cinttypes>

namespace NEO {

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitWidthOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed) {
        return 1024;
    }
    return 0;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed) {
        return 1024;
    }
    return 0;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;

    appendClearColor(blitProperties, blitCmd);

    uint32_t compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);
    if (DebugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        compressionFormat = DebugManager.flags.ForceBufferCompressionFormat.get();
    }

    auto compressionEnabledField = XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE;
    if (DebugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.get() != -1) {
        compressionEnabledField = static_cast<typename XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE>(DebugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.get());
    }

    if (blitProperties.dstAllocation->isCompressionEnabled()) {
        blitCmd.setDestinationCompressionEnable(compressionEnabledField);
        blitCmd.setDestinationAuxiliarysurfacemode(XY_BLOCK_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        blitCmd.setDestinationCompressionFormat(compressionFormat);
    }
    if (blitProperties.srcAllocation->isCompressionEnabled()) {
        blitCmd.setSourceCompressionEnable(compressionEnabledField);
        blitCmd.setSourceAuxiliarysurfacemode(XY_BLOCK_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        blitCmd.setSourceCompressionFormat(compressionFormat);
    }

    blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);

    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);

    blitCmd.setSourceSurfaceWidth(blitCmd.getDestinationX2CoordinateRight());
    blitCmd.setSourceSurfaceHeight(blitCmd.getDestinationY2CoordinateBottom());

    blitCmd.setDestinationSurfaceWidth(blitCmd.getDestinationX2CoordinateRight());
    blitCmd.setDestinationSurfaceHeight(blitCmd.getDestinationY2CoordinateBottom());

    if (blitCmd.getDestinationY2CoordinateBottom() > 1) {
        blitCmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
        blitCmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
    } else {
        blitCmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
        blitCmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
    }

    if (AuxTranslationDirection::AuxToNonAux == blitProperties.auxTranslationDirection) {
        blitCmd.setSpecialModeofOperation(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE);
    } else if (AuxTranslationDirection::NonAuxToAux == blitProperties.auxTranslationDirection) {
        blitCmd.setSourceCompressionEnable(XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
    }

    DEBUG_BREAK_IF((AuxTranslationDirection::None != blitProperties.auxTranslationDirection) &&
                   (blitProperties.dstAllocation != blitProperties.srcAllocation || !blitProperties.dstAllocation->isCompressionEnabled()));

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
    blitCmd.setDestinationMOCS(mocs);
    blitCmd.setSourceMOCS(mocs);

    if (DebugManager.flags.OverrideBlitterMocs.get() != -1) {
        blitCmd.setDestinationMOCS(DebugManager.flags.OverrideBlitterMocs.get());
        blitCmd.setSourceMOCS(DebugManager.flags.OverrideBlitterMocs.get());
    }
    if (DebugManager.flags.OverrideBlitterTargetMemory.get() != -1) {
        if (DebugManager.flags.OverrideBlitterTargetMemory.get() == 0u) {
            blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
            blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        } else if (DebugManager.flags.OverrideBlitterTargetMemory.get() == 1u) {
            blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
            blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        }
    }
}

template <typename GfxFamily>
void setCompressionParamsForFillOperation(typename GfxFamily::XY_COLOR_BLT &xyColorBlt) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    xyColorBlt.setDestinationCompressionEnable(XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_ENABLE);
    xyColorBlt.setDestinationAuxiliarysurfacemode(XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForFillBuffer(NEO::GraphicsAllocation *dstAlloc, typename GfxFamily::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    uint32_t compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);
    if (DebugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        compressionFormat = DebugManager.flags.ForceBufferCompressionFormat.get();
    }

    if (dstAlloc->isCompressionEnabled()) {
        setCompressionParamsForFillOperation<GfxFamily>(blitCmd);
        blitCmd.setDestinationCompressionFormat(compressionFormat);
    }

    blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);

    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
    blitCmd.setDestinationMOCS(mocs);
    if (DebugManager.flags.OverrideBlitterMocs.get() != -1) {
        blitCmd.setDestinationMOCS(DebugManager.flags.OverrideBlitterMocs.get());
    }

    if (DebugManager.flags.OverrideBlitterTargetMemory.get() != -1) {
        if (DebugManager.flags.OverrideBlitterTargetMemory.get() == 0u) {
            blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
        } else if (DebugManager.flags.OverrideBlitterTargetMemory.get() == 1u) {
            blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(NEO::GraphicsAllocation *dstAlloc, uint64_t offset, uint32_t *pattern, size_t patternSize, LinearStream &linearStream, size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) {
    switch (patternSize) {
    case 1:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<1>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
        break;
    case 2:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<2>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
        break;
    case 4:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<4>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
        break;
    case 8:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<8>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
        break;
    default:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<16>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;

    if (blitProperties.srcAllocation->getDefaultGmm()) {
        auto resInfo = blitProperties.srcAllocation->getDefaultGmm()->gmmResourceInfo.get();
        auto resourceType = resInfo->getResourceType();
        auto isArray = resInfo->getArraySize() > 1;

        if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_1D) {
            if (isArray) {
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

        if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_1D) {
            if (isArray) {
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
                                                                uint32_t &compressionType, const RootDeviceEnvironment &rootDeviceEnvironment,
                                                                GMM_YUV_PLANE_ENUM plane) {
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;

    if (allocation.getDefaultGmm()) {
        auto gmmResourceInfo = allocation.getDefaultGmm()->gmmResourceInfo.get();
        mipTailLod = gmmResourceInfo->getMipTailStartLodSurfaceState();
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
            compressionType = XY_BLOCK_COPY_BLT::COMPRESSION_TYPE::COMPRESSION_TYPE_MEDIA_COMPRESSION;
        } else if (resInfo.RenderCompressed) {
            compressionDetails = gmmClientContext->getSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
            compressionType = XY_BLOCK_COPY_BLT::COMPRESSION_TYPE::COMPRESSION_TYPE_3D_COMPRESSION;
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForImages(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch) {
    using COMPRESSION_TYPE = typename GfxFamily::XY_BLOCK_COPY_BLT::COMPRESSION_TYPE;
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
    auto srcCompressionType = static_cast<uint32_t>(blitCmd.getSourceCompressionType());
    auto dstCompressionType = static_cast<uint32_t>(blitCmd.getDestinationCompressionType());

    getBlitAllocationProperties(*srcAllocation, srcRowPitch, srcQPitch, srcTileType, srcMipTailLod, srcCompressionFormat,
                                srcCompressionType, rootDeviceEnvironment, blitProperties.srcPlane);
    getBlitAllocationProperties(*dstAllocation, dstRowPitch, dstQPitch, dstTileType, dstMipTailLod, dstCompressionFormat,
                                dstCompressionType, rootDeviceEnvironment, blitProperties.dstPlane);

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
    blitCmd.setSourceCompressionType(static_cast<COMPRESSION_TYPE>(srcCompressionType));
    blitCmd.setDestinationCompressionType(static_cast<COMPRESSION_TYPE>(dstCompressionType));

    appendTilingType(srcTileType, dstTileType, blitCmd);
    appendClearColor(blitProperties, blitCmd);
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
    blitCmd.setDestinationSurfaceType(XY_COLOR_BLT::DESTINATION_SURFACE_TYPE::DESTINATION_SURFACE_TYPE_2D);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::programGlobalSequencerFlush(LinearStream &commandStream) {
    if (DebugManager.flags.GlobalSequencerFlushOnCopyEngine.get() != 0) {
        using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
        constexpr uint32_t globalInvalidationRegister = 0xB404u;
        LriHelper<GfxFamily>::program(&commandStream, globalInvalidationRegister, 1u, false);
        EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(commandStream,
                                                              globalInvalidationRegister,
                                                              0u,
                                                              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                              true);
    }
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getSizeForGlobalSequencerFlush() {
    if (DebugManager.flags.GlobalSequencerFlushOnCopyEngine.get() != 0) {
        return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM) + sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT);
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

} // namespace NEO
