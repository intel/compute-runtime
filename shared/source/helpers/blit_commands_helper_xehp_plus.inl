/*
 * Copyright (C) 2021 Intel Corporation
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

#include "opencl/source/helpers/hardware_commands_helper.h"

namespace NEO {

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitWidthOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    if (hwHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed) {
        return 1024;
    }
    return 0;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    if (hwHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed) {
        return 1024;
    }
    return 0;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForBuffer(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    auto dstAllocation = blitProperties.dstAllocation;
    auto srcAllocation = blitProperties.srcAllocation;
    bool dstAllocationisCompressionEnabled = dstAllocation->getDefaultGmm() && dstAllocation->getDefaultGmm()->isCompressionEnabled;
    bool srcAllocationisCompressionEnabled = srcAllocation->getDefaultGmm() && srcAllocation->getDefaultGmm()->isCompressionEnabled;

    appendClearColor(blitProperties, blitCmd);

    uint32_t compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);
    if (DebugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        compressionFormat = DebugManager.flags.ForceBufferCompressionFormat.get();
    }

    auto compressionEnabledField = XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE;
    if (DebugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.get() != -1) {
        compressionEnabledField = static_cast<typename XY_COPY_BLT::COMPRESSION_ENABLE>(DebugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.get());
    }

    if (dstAllocationisCompressionEnabled) {
        blitCmd.setDestinationCompressionEnable(compressionEnabledField);
        blitCmd.setDestinationAuxiliarysurfacemode(XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        blitCmd.setDestinationCompressionFormat(compressionFormat);
    }
    if (srcAllocationisCompressionEnabled) {
        blitCmd.setSourceCompressionEnable(compressionEnabledField);
        blitCmd.setSourceAuxiliarysurfacemode(XY_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        blitCmd.setSourceCompressionFormat(compressionFormat);
    }

    if (MemoryPool::isSystemMemoryPool(dstAllocation->getMemoryPool())) {
        blitCmd.setDestinationTargetMemory(XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
    if (MemoryPool::isSystemMemoryPool(srcAllocation->getMemoryPool())) {
        blitCmd.setSourceTargetMemory(XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }

    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);

    blitCmd.setSourceSurfaceWidth(blitCmd.getDestinationX2CoordinateRight());
    blitCmd.setSourceSurfaceHeight(blitCmd.getDestinationY2CoordinateBottom());

    blitCmd.setDestinationSurfaceWidth(blitCmd.getDestinationX2CoordinateRight());
    blitCmd.setDestinationSurfaceHeight(blitCmd.getDestinationY2CoordinateBottom());

    if (blitCmd.getDestinationY2CoordinateBottom() > 1) {
        blitCmd.setDestinationSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
        blitCmd.setSourceSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
    } else {
        blitCmd.setDestinationSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
        blitCmd.setSourceSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
    }

    if (AuxTranslationDirection::AuxToNonAux == blitProperties.auxTranslationDirection) {
        blitCmd.setSpecialModeofOperation(XY_COPY_BLT::SPECIAL_MODE_OF_OPERATION::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE);
    } else if (AuxTranslationDirection::NonAuxToAux == blitProperties.auxTranslationDirection) {
        blitCmd.setSourceCompressionEnable(XY_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
    }

    DEBUG_BREAK_IF((AuxTranslationDirection::None != blitProperties.auxTranslationDirection) &&
                   (dstAllocation != srcAllocation || !dstAllocationisCompressionEnabled));

    auto mocsIndex = rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    blitCmd.setDestinationMOCSvalue(mocsIndex);
    blitCmd.setSourceMOCS(mocsIndex);
    if (DebugManager.flags.OverrideBlitterMocs.get() != -1) {
        blitCmd.setDestinationMOCSvalue(DebugManager.flags.OverrideBlitterMocs.get());
        blitCmd.setSourceMOCS(DebugManager.flags.OverrideBlitterMocs.get());
    }
    if (DebugManager.flags.OverrideBlitterTargetMemory.get() != -1) {
        if (DebugManager.flags.OverrideBlitterTargetMemory.get() == 0u) {
            blitCmd.setDestinationTargetMemory(XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
            blitCmd.setSourceTargetMemory(XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        } else if (DebugManager.flags.OverrideBlitterTargetMemory.get() == 1u) {
            blitCmd.setDestinationTargetMemory(XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
            blitCmd.setSourceTargetMemory(XY_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
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
    bool dstAllocationisCompressionEnabled = dstAlloc->getDefaultGmm() && dstAlloc->getDefaultGmm()->isCompressionEnabled;

    uint32_t compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);
    if (DebugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        compressionFormat = DebugManager.flags.ForceBufferCompressionFormat.get();
    }

    if (dstAllocationisCompressionEnabled) {
        setCompressionParamsForFillOperation<GfxFamily>(blitCmd);
        blitCmd.setDestinationCompressionFormat(compressionFormat);
    }

    if (MemoryPool::isSystemMemoryPool(dstAlloc->getMemoryPool())) {
        blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
    }

    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);

    auto mocsIndex = rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    blitCmd.setDestinationMOCSvalue(mocsIndex);
    if (DebugManager.flags.OverrideBlitterMocs.get() != -1) {
        blitCmd.setDestinationMOCSvalue(DebugManager.flags.OverrideBlitterMocs.get());
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
void BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(NEO::GraphicsAllocation *dstAlloc, uint32_t *pattern, size_t patternSize, LinearStream &linearStream, size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) {
    switch (patternSize) {
    case 1:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<1>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
        break;
    case 2:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<2>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
        break;
    case 4:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<4>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
        break;
    case 8:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<8>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
        break;
    default:
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryFill<16>(dstAlloc, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd) {
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    if (blitProperties.srcAllocation->getDefaultGmm()) {
        auto resInfo = blitProperties.srcAllocation->getDefaultGmm()->gmmResourceInfo.get();
        auto resourceType = resInfo->getResourceType();
        auto isArray = resInfo->getArraySize() > 1;

        if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_1D) {
            if (isArray) {
                blitCmd.setSourceSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
            } else {
                blitCmd.setSourceSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
            }

        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_2D) {
            blitCmd.setSourceSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
            blitCmd.setSourceSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D);
        }
    }

    if (blitProperties.dstAllocation->getDefaultGmm()) {
        auto resInfo = blitProperties.dstAllocation->getDefaultGmm()->gmmResourceInfo.get();
        auto resourceType = resInfo->getResourceType();
        auto isArray = resInfo->getArraySize() > 1;

        if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_1D) {
            if (isArray) {
                blitCmd.setDestinationSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
            } else {
                blitCmd.setDestinationSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
            }
        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_2D) {
            blitCmd.setDestinationSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
        } else if (resourceType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
            blitCmd.setDestinationSurfaceType(XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D);
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingType(const GMM_TILE_TYPE srcTilingType, const GMM_TILE_TYPE dstTilingType, typename GfxFamily::XY_COPY_BLT &blitCmd) {
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    if (srcTilingType == GMM_TILED_4) {
        blitCmd.setSourceTiling(XY_COPY_BLT::TILING::TILING_TILE4);
    } else if (srcTilingType == GMM_TILED_64) {
        blitCmd.setSourceTiling(XY_COPY_BLT::TILING::TILING_TILE64);
    } else {
        blitCmd.setSourceTiling(XY_COPY_BLT::TILING::TILING_LINEAR);
    }
    if (dstTilingType == GMM_TILED_4) {
        blitCmd.setDestinationTiling(XY_COPY_BLT::TILING::TILING_TILE4);
    } else if (dstTilingType == GMM_TILED_64) {
        blitCmd.setDestinationTiling(XY_COPY_BLT::TILING::TILING_TILE64);
    } else {
        blitCmd.setDestinationTiling(XY_COPY_BLT::TILING::TILING_LINEAR);
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendColorDepth(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd) {
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    switch (blitProperties.bytesPerPixel) {
    default:
        UNRECOVERABLE_IF(true);
    case 1:
        blitCmd.setColorDepth(XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
        break;
    case 2:
        blitCmd.setColorDepth(XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
        break;
    case 4:
        blitCmd.setColorDepth(XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
        break;
    case 8:
        blitCmd.setColorDepth(XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
        break;
    case 16:
        blitCmd.setColorDepth(XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
        break;
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::getBlitAllocationProperties(const GraphicsAllocation &allocation, uint32_t &pitch, uint32_t &qPitch, GMM_TILE_TYPE &tileType, uint32_t &mipTailLod, uint32_t &compressionDetails, const RootDeviceEnvironment &rootDeviceEnvironment) {
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
        } else if (resInfo.RenderCompressed) {
            compressionDetails = gmmClientContext->getSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());
        }
    }
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForImages(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch) {
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

    getBlitAllocationProperties(*srcAllocation, srcRowPitch, srcQPitch, srcTileType, srcMipTailLod, srcCompressionFormat, rootDeviceEnvironment);
    getBlitAllocationProperties(*dstAllocation, dstRowPitch, dstQPitch, dstTileType, dstMipTailLod, dstCompressionFormat, rootDeviceEnvironment);

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
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSliceOffsets(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd, uint32_t sliceIndex, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t srcSlicePitch, uint32_t dstSlicePitch) {
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    auto srcAddress = blitProperties.srcGpuAddress;
    auto dstAddress = blitProperties.dstGpuAddress;

    if (blitCmd.getSourceTiling() == XY_COPY_BLT::TILING::TILING_LINEAR) {
        blitCmd.setSourceBaseAddress(ptrOffset(srcAddress, srcSlicePitch * (sliceIndex + blitProperties.srcOffset.z)));
    } else {
        blitCmd.setSourceArrayIndex((sliceIndex + static_cast<uint32_t>(blitProperties.srcOffset.z)) + 1);
    }
    if (blitCmd.getDestinationTiling() == XY_COPY_BLT::TILING::TILING_LINEAR) {
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
void BlitCommandsHelper<GfxFamily>::appendClearColor(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd) {
}

} // namespace NEO
