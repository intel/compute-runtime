/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;

    uint8_t srcCompressionFormat = 0;
    uint8_t dstCompressionFormat = 0;
    auto dstAllocation = blitProperties.dstAllocation;
    auto srcAllocation = blitProperties.srcAllocation;

    if (srcAllocation && srcAllocation->isCompressionEnabled()) {
        auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        srcCompressionFormat = static_cast<uint8_t>(rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat));
        if (auto bcsCompressionFormat = rootDeviceEnvironment.getHelper<ProductHelper>().getBcsCompressionFormat()) {
            srcCompressionFormat = *bcsCompressionFormat;
        }
    }

    if (dstAllocation && dstAllocation->isCompressionEnabled()) {
        auto resourceFormat = dstAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        dstCompressionFormat = static_cast<uint8_t>(rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat));
        if (auto bcsCompressionFormat = rootDeviceEnvironment.getHelper<ProductHelper>().getBcsCompressionFormat()) {
            dstCompressionFormat = *bcsCompressionFormat;
        }
    }

    if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        if (srcAllocation && srcAllocation->isCompressionEnabled()) {
            srcCompressionFormat = static_cast<uint8_t>(debugManager.flags.ForceBufferCompressionFormat.get());
        }
        if (dstAllocation && dstAllocation->isCompressionEnabled()) {
            dstCompressionFormat = static_cast<uint8_t>(debugManager.flags.ForceBufferCompressionFormat.get());
        }
    }

    DEBUG_BREAK_IF((AuxTranslationDirection::none != blitProperties.auxTranslationDirection) &&
                   (blitProperties.dstAllocation != blitProperties.srcAllocation || (blitProperties.dstAllocation && !blitProperties.dstAllocation->isCompressionEnabled())));

    blitCmd.setSourceCompressionFormat(static_cast<XY_BLOCK_COPY_BLT::SOURCE_COMPRESSION_FORMAT>(srcCompressionFormat));
    blitCmd.setDestinationCompressionFormat(static_cast<XY_BLOCK_COPY_BLT::DESTINATION_COMPRESSION_FORMAT>(dstCompressionFormat));

    if (!dstAllocation || MemoryPoolHelper::isSystemMemoryPool(dstAllocation->getMemoryPool())) {
        blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
    if (!srcAllocation || MemoryPoolHelper::isSystemMemoryPool(srcAllocation->getMemoryPool())) {
        blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }

    if (debugManager.flags.OverrideBlitterTargetMemory.get() != -1) {
        if (debugManager.flags.OverrideBlitterTargetMemory.get() == 0u) {
            blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
            blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        } else if (debugManager.flags.OverrideBlitterTargetMemory.get() == 1u) {
            blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
            blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        }
    }

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

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getL3EnabledMOCS();
    if (debugManager.flags.OverrideBlitterMocs.get() != -1) {
        mocs = static_cast<uint32_t>(debugManager.flags.OverrideBlitterMocs.get());
    }

    blitCmd.setDestinationMOCS(mocs);
    blitCmd.setSourceMOCS(mocs);
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsMemCopy(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd,
                                                           const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MEM_COPY = typename Family::MEM_COPY;
    using COMPRESSION_FORMAT = typename MEM_COPY::COMPRESSION_FORMAT;

    auto dstAllocation = blitProperties.dstAllocation;
    auto srcAllocation = blitProperties.srcAllocation;

    if (blitCmd.getDestinationY2CoordinateBottom() > 1) {
        blitCmd.setCopyType(MEM_COPY::COPY_TYPE::COPY_TYPE_MATRIX_COPY);
    } else {
        blitCmd.setCopyType(MEM_COPY::COPY_TYPE::COPY_TYPE_LINEAR_COPY);
    }

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getL3EnabledMOCS();
    if (debugManager.flags.OverrideBlitterMocs.get() != -1) {
        mocs = static_cast<uint32_t>(debugManager.flags.OverrideBlitterMocs.get());
    }
    blitCmd.setDestinationMOCS(mocs);
    blitCmd.setSourceMOCS(mocs);

    uint8_t compressionFormat = 0;

    if (dstAllocation) {
        if (dstAllocation->isCompressionEnabled()) {
            auto resourceFormat = dstAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
            compressionFormat = static_cast<uint8_t>(rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat));
        }
    }

    if (compressionFormat == 0) {
        if (srcAllocation) {
            if (srcAllocation->isCompressionEnabled()) {
                auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
                compressionFormat = static_cast<uint8_t>(rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat));
            }
        }
    }

    if (compressionFormat != 0) {
        if (auto bcsCompressionFormat = rootDeviceEnvironment.getHelper<ProductHelper>().getBcsCompressionFormat()) {
            compressionFormat = *bcsCompressionFormat;
        }
    }

    if (debugManager.flags.BcsCompressionFormatForXe2Plus.get() != -1) {
        compressionFormat = static_cast<uint8_t>(debugManager.flags.BcsCompressionFormatForXe2Plus.get());
    }

    blitCmd.setCompressionFormat(static_cast<COMPRESSION_FORMAT>(compressionFormat));

    DEBUG_BREAK_IF(AuxTranslationDirection::none != blitProperties.auxTranslationDirection);
}
