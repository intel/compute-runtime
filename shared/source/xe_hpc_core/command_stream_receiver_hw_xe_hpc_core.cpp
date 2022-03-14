/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/memory_fence_encoder.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/source/xe_hpc_core/hw_info.h"

using Family = NEO::XE_HPC_COREFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_dg2_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_tgllp_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xehp_and_later.inl"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {
static auto gfxCore = IGFX_XE_HPC_CORE;

template <>
bool ImplicitFlushSettings<Family>::defaultSettingForNewResource = false;
template <>
bool ImplicitFlushSettings<Family>::defaultSettingForGpuIdle = false;
template class ImplicitFlushSettings<Family>;

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

template <>
void CommandStreamReceiverHw<Family>::programEnginePrologue(LinearStream &csr) {
    if (!this->isEnginePrologueSent) {
        if (globalFenceAllocation) {
            EncodeMemoryFence<Family>::encodeSystemMemoryFence(csr, globalFenceAllocation);
        }
        this->isEnginePrologueSent = true;
    }
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPrologue() const {
    if (!this->isEnginePrologueSent) {
        if (globalFenceAllocation) {
            return sizeof(Family::STATE_SYSTEM_MEM_FENCE_ADDRESS);
        }
    }
    return 0u;
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <>
template <typename T>
void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer(const BlitProperties &blitProperties, T &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    appendBlitCommandsMemCopy(blitProperties, blitCmd, rootDeviceEnvironment);
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsMemCopy(const BlitProperties &blitProperites, typename Family::XY_COPY_BLT &blitCmd,
                                                           const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MEM_COPY = typename Family::MEM_COPY;

    auto dstAllocation = blitProperites.dstAllocation;
    auto srcAllocation = blitProperites.srcAllocation;

    if (blitCmd.getDestinationY2CoordinateBottom() > 1) {
        blitCmd.setCopyType(MEM_COPY::COPY_TYPE::COPY_TYPE_MATRIX_COPY);
    } else {
        blitCmd.setCopyType(MEM_COPY::COPY_TYPE::COPY_TYPE_LINEAR_COPY);
    }

    auto mocsL3enabled = rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    blitCmd.setDestinationMOCS(mocsL3enabled);
    blitCmd.setSourceMOCS(mocsL3enabled);
    if (DebugManager.flags.OverrideBlitterMocs.get() != -1) {
        blitCmd.setDestinationMOCS(DebugManager.flags.OverrideBlitterMocs.get());
        blitCmd.setSourceMOCS(DebugManager.flags.OverrideBlitterMocs.get());
    }

    if (dstAllocation->isCompressionEnabled()) {
        auto resourceFormat = dstAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
        blitCmd.setDestinationCompressible(MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
        blitCmd.setCompressionFormat(compressionFormat);
    }
    if (srcAllocation->isCompressionEnabled()) {
        auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
        blitCmd.setSourceCompressible(MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
        blitCmd.setCompressionFormat(compressionFormat);
    }

    if (DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (!MemoryPool::isSystemMemoryPool(srcAllocation->getMemoryPool())) {
            blitCmd.setSourceCompressible(MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
            blitCmd.setCompressionFormat(DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
        }
        if (!MemoryPool::isSystemMemoryPool(dstAllocation->getMemoryPool())) {
            blitCmd.setDestinationCompressible(MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
            blitCmd.setCompressionFormat(DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
        }
    }

    if (blitCmd.getDestinationCompressible() == MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE &&
        AuxTranslationDirection::AuxToNonAux != blitProperites.auxTranslationDirection) {
        blitCmd.setDestinationCompressionEnable(MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    } else {
        blitCmd.setDestinationCompressionEnable(MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    }

    DEBUG_BREAK_IF((AuxTranslationDirection::None != blitProperites.auxTranslationDirection) &&
                   (dstAllocation != srcAllocation || !dstAllocation->isCompressionEnabled()));
}

template <>
template <>
void BlitCommandsHelper<Family>::dispatchBlitMemoryFill<1>(NEO::GraphicsAllocation *dstAlloc, uint64_t offset, uint32_t *pattern, LinearStream &linearStream, size_t size, const RootDeviceEnvironment &rootDeviceEnvironment, COLOR_DEPTH depth) {
    using MEM_SET = typename Family::MEM_SET;
    auto blitCmd = Family::cmdInitMemSet;

    auto mocsL3enabled = rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    blitCmd.setDestinationMOCS(mocsL3enabled);
    if (DebugManager.flags.OverrideBlitterMocs.get() != -1) {
        blitCmd.setDestinationMOCS(DebugManager.flags.OverrideBlitterMocs.get());
    }

    if (dstAlloc->isCompressionEnabled()) {
        auto resourceFormat = dstAlloc->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
        blitCmd.setDestinationCompressible(MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
        blitCmd.setCompressionFormat40(compressionFormat);
    }
    if (DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (!MemoryPool::isSystemMemoryPool(dstAlloc->getMemoryPool())) {
            blitCmd.setDestinationCompressible(MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
            blitCmd.setCompressionFormat40(DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
        }
    }

    if (blitCmd.getDestinationCompressible() == MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE) {
        blitCmd.setDestinationCompressionEnable(MEM_SET::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    } else {
        blitCmd.setDestinationCompressionEnable(MEM_SET::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    }
    blitCmd.setFillData(*pattern);

    auto sizeToFill = size;
    while (sizeToFill != 0) {
        auto tmpCmd = blitCmd;
        tmpCmd.setDestinationStartAddress(ptrOffset(dstAlloc->getGpuAddress(), static_cast<size_t>(offset)));
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

        auto cmd = linearStream.getSpaceForCmd<MEM_SET>();
        *cmd = tmpCmd;

        auto blitSize = width * height;
        offset += blitSize;
        sizeToFill -= blitSize;
    }
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsForImages(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch) {}

template <>
void BlitCommandsHelper<Family>::appendTilingType(const GMM_TILE_TYPE srcTilingType, const GMM_TILE_TYPE dstTilingType, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {}

template <>
void BlitCommandsHelper<Family>::appendSliceOffsets(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, uint32_t sliceIndex, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t srcSlicePitch, uint32_t dstSlicePitch) {}

template <>
void BlitCommandsHelper<Family>::appendSurfaceType(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {
}

template <>
template <>
void BlitCommandsHelper<Family>::appendColorDepth(const BlitProperties &blitProperites, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {}

template <>
template <>
void BlitCommandsHelper<Family>::appendColorDepth(const BlitProperties &blitProperites, typename Family::XY_COPY_BLT &blitCmd) {}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;
template void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);

const Family::COMPUTE_WALKER Family::cmdInitGpgpuWalker = Family::COMPUTE_WALKER::sInit();
const Family::CFE_STATE Family::cmdInitCfeState = Family::CFE_STATE::sInit();
const Family::INTERFACE_DESCRIPTOR_DATA Family::cmdInitInterfaceDescriptorData = Family::INTERFACE_DESCRIPTOR_DATA::sInit();
const Family::MI_BATCH_BUFFER_START Family::cmdInitBatchBufferStart = Family::MI_BATCH_BUFFER_START::sInit();
const Family::MI_BATCH_BUFFER_END Family::cmdInitBatchBufferEnd = Family::MI_BATCH_BUFFER_END::sInit();
const Family::PIPE_CONTROL Family::cmdInitPipeControl = Family::PIPE_CONTROL::sInit();
const Family::STATE_COMPUTE_MODE Family::cmdInitStateComputeMode = Family::STATE_COMPUTE_MODE::sInit();
const Family::_3DSTATE_BINDING_TABLE_POOL_ALLOC Family::cmdInitStateBindingTablePoolAlloc =
    Family::_3DSTATE_BINDING_TABLE_POOL_ALLOC::sInit();
const Family::MI_SEMAPHORE_WAIT Family::cmdInitMiSemaphoreWait = Family::MI_SEMAPHORE_WAIT::sInit();
const Family::RENDER_SURFACE_STATE Family::cmdInitRenderSurfaceState = Family::RENDER_SURFACE_STATE::sInit();
const Family::POSTSYNC_DATA Family::cmdInitPostSyncData = Family::POSTSYNC_DATA::sInit();
const Family::MI_SET_PREDICATE Family::cmdInitSetPredicate = Family::MI_SET_PREDICATE::sInit();
const Family::MI_LOAD_REGISTER_IMM Family::cmdInitLoadRegisterImm = Family::MI_LOAD_REGISTER_IMM::sInit();
const Family::MI_LOAD_REGISTER_REG Family::cmdInitLoadRegisterReg = Family::MI_LOAD_REGISTER_REG::sInit();
const Family::MI_LOAD_REGISTER_MEM Family::cmdInitLoadRegisterMem = Family::MI_LOAD_REGISTER_MEM::sInit();
const Family::MI_STORE_DATA_IMM Family::cmdInitStoreDataImm = Family::MI_STORE_DATA_IMM::sInit();
const Family::MI_STORE_REGISTER_MEM Family::cmdInitStoreRegisterMem = Family::MI_STORE_REGISTER_MEM::sInit();
const Family::MI_NOOP Family::cmdInitNoop = Family::MI_NOOP::sInit();
const Family::MI_REPORT_PERF_COUNT Family::cmdInitReportPerfCount = Family::MI_REPORT_PERF_COUNT::sInit();
const Family::MI_ATOMIC Family::cmdInitAtomic = Family::MI_ATOMIC::sInit();
const Family::PIPELINE_SELECT Family::cmdInitPipelineSelect = Family::PIPELINE_SELECT::sInit();
const Family::MI_ARB_CHECK Family::cmdInitArbCheck = Family::MI_ARB_CHECK::sInit();
const Family::STATE_BASE_ADDRESS Family::cmdInitStateBaseAddress = Family::STATE_BASE_ADDRESS::sInit();
const Family::MEDIA_SURFACE_STATE Family::cmdInitMediaSurfaceState = Family::MEDIA_SURFACE_STATE::sInit();
const Family::SAMPLER_STATE Family::cmdInitSamplerState = Family::SAMPLER_STATE::sInit();
const Family::BINDING_TABLE_STATE Family::cmdInitBindingTableState = Family::BINDING_TABLE_STATE::sInit();
const Family::MI_USER_INTERRUPT Family::cmdInitUserInterrupt = Family::MI_USER_INTERRUPT::sInit();
const Family::MI_CONDITIONAL_BATCH_BUFFER_END cmdInitConditionalBatchBufferEnd = Family::MI_CONDITIONAL_BATCH_BUFFER_END::sInit();
const Family::MI_FLUSH_DW Family::cmdInitMiFlushDw = Family::MI_FLUSH_DW::sInit();
const Family::XY_BLOCK_COPY_BLT Family::cmdInitXyBlockCopyBlt = Family::XY_BLOCK_COPY_BLT::sInit();
const Family::MEM_COPY Family::cmdInitXyCopyBlt = Family::MEM_COPY::sInit();
const Family::XY_FAST_COLOR_BLT Family::cmdInitXyColorBlt = Family::XY_FAST_COLOR_BLT::sInit();
const Family::STATE_PREFETCH Family::cmdInitStatePrefetch = Family::STATE_PREFETCH::sInit();
const Family::_3DSTATE_BTD Family::cmd3dStateBtd = Family::_3DSTATE_BTD::sInit();
const Family::_3DSTATE_BTD_BODY Family::cmd3dStateBtdBody = Family::_3DSTATE_BTD_BODY::sInit();
const Family::MI_MEM_FENCE Family::cmdInitMemFence = Family::MI_MEM_FENCE::sInit();
const Family::MEM_SET Family::cmdInitMemSet = Family::MEM_SET::sInit();
const Family::STATE_SIP Family::cmdInitStateSip = Family::STATE_SIP::sInit();
const Family::STATE_SYSTEM_MEM_FENCE_ADDRESS Family::cmdInitStateSystemMemFenceAddress = Family::STATE_SYSTEM_MEM_FENCE_ADDRESS::sInit();
} // namespace NEO
