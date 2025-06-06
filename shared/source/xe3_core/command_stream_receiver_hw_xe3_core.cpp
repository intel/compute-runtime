/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

using Family = NEO::Xe3CoreFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_dg2_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_heap_addressing.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/blit_commands_helper_from_gen12lp_to_xe3.inl"
#include "shared/source/helpers/blit_commands_helper_pvc_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xe2_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xehp_and_later.inl"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/allocation_properties.h"

namespace NEO {
static auto gfxCore = IGFX_XE3_CORE;

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
        if (getGlobalFenceAllocation()) {
            EncodeMemoryFence<Family>::encodeSystemMemoryFence(csr, getGlobalFenceAllocation());
        }
        this->isEnginePrologueSent = true;
    }
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPrologue() const {
    if (!this->isEnginePrologueSent) {
        if (getGlobalFenceAllocation()) {
            return EncodeMemoryFence<Family>::getSystemMemoryFenceSize();
        }
    }
    return 0u;
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;

    uint8_t srcCompressionFormat = 0;
    uint8_t dstCompressionFormat = 0;
    auto dstAllocation = blitProperties.dstAllocation;
    auto srcAllocation = blitProperties.srcAllocation;

    if (srcAllocation->isCompressionEnabled()) {
        auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        srcCompressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    }

    if (dstAllocation->isCompressionEnabled()) {
        auto resourceFormat = dstAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        dstCompressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    }

    if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        if (srcAllocation->isCompressionEnabled()) {
            srcCompressionFormat = static_cast<uint8_t>(debugManager.flags.ForceBufferCompressionFormat.get());
        }
        if (dstAllocation->isCompressionEnabled()) {
            dstCompressionFormat = static_cast<uint8_t>(debugManager.flags.ForceBufferCompressionFormat.get());
        }
    }

    DEBUG_BREAK_IF((AuxTranslationDirection::none != blitProperties.auxTranslationDirection) &&
                   (blitProperties.dstAllocation != blitProperties.srcAllocation || !blitProperties.dstAllocation->isCompressionEnabled()));

    blitCmd.setSourceCompressionFormat(static_cast<XY_BLOCK_COPY_BLT::SOURCE_COMPRESSION_FORMAT>(srcCompressionFormat));
    blitCmd.setDestinationCompressionFormat(static_cast<XY_BLOCK_COPY_BLT::DESTINATION_COMPRESSION_FORMAT>(dstCompressionFormat));

    if (MemoryPoolHelper::isSystemMemoryPool(blitProperties.dstAllocation->getMemoryPool())) {
        blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
    if (MemoryPoolHelper::isSystemMemoryPool(blitProperties.srcAllocation->getMemoryPool())) {
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
template <typename T>
void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer(const BlitProperties &blitProperties, T &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    appendBlitCommandsMemCopy(blitProperties, blitCmd, rootDeviceEnvironment);
}

template <>
uint32_t BlitCommandsHelper<Family>::getAvailableBytesPerPixel(size_t copySize, uint32_t srcOrigin, uint32_t dstOrigin, size_t srcSize, size_t dstSize) {
    return 1;
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsMemCopy(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd,
                                                           const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MEM_COPY = typename Family::MEM_COPY;
    using COMPRESSION_FORMAT30 = typename MEM_COPY::COMPRESSION_FORMAT30;

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
            compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
        }
    }
    if (compressionFormat == 0) {
        if (srcAllocation) {
            if (srcAllocation->isCompressionEnabled()) {
                auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
                compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
            }
        }
    }

    if (debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        bool enable = false;
        if (srcAllocation) {
            if (!MemoryPoolHelper::isSystemMemoryPool(srcAllocation->getMemoryPool())) {
                enable = true;
            }
        }
        if (dstAllocation) {
            if (!MemoryPoolHelper::isSystemMemoryPool(dstAllocation->getMemoryPool())) {
                enable = true;
            }
        }
        if (enable) {
            compressionFormat = static_cast<uint8_t>(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
        }
    }

    blitCmd.setCompressionFormat(static_cast<COMPRESSION_FORMAT30>(compressionFormat));

    DEBUG_BREAK_IF(AuxTranslationDirection::none != blitProperties.auxTranslationDirection);
}

template <>
void BlitCommandsHelper<Family>::encodeProfilingStartMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextStartGpuAddress = TimestampPacketHelper::getContextStartGpuAddress(timestampPacketNode);
    auto timestampGlobalStartAddress = TimestampPacketHelper::getGlobalStartGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, timestampContextStartGpuAddress + sizeof(uint32_t), false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampUn, timestampGlobalStartAddress + sizeof(uint32_t), false, nullptr, true);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextStartGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalStartAddress, false, nullptr, true);
}

template <>
void BlitCommandsHelper<Family>::encodeProfilingEndMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextEndGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(timestampPacketNode);
    auto timestampGlobalEndAddress = TimestampPacketHelper::getGlobalEndGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, timestampContextEndGpuAddress + sizeof(uint32_t), false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampUn, timestampGlobalEndAddress + sizeof(uint32_t), false, nullptr, true);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextEndGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalEndAddress, false, nullptr, true);
}

template <>
size_t BlitCommandsHelper<Family>::getProfilingMmioCmdsSize() {
    return 8 * sizeof(typename Family::MI_STORE_REGISTER_MEM);
}

template <>
void setCompressionParamsForFillOperation<Family>(typename Family::XY_COLOR_BLT &xyColorBlt) {
}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;
template void BlitCommandsHelper<Family>::appendColorDepth<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd);
template void BlitCommandsHelper<Family>::appendColorDepth<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd);
template void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);

template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last);
template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last);

const Family::COMPUTE_WALKER Family::cmdInitGpgpuWalker = Family::COMPUTE_WALKER::sInit();
const Family::CFE_STATE Family::cmdInitCfeState = Family::CFE_STATE::sInit();
const Family::INTERFACE_DESCRIPTOR_DATA Family::cmdInitInterfaceDescriptorData = Family::INTERFACE_DESCRIPTOR_DATA::sInit();
const Family::MI_BATCH_BUFFER_START Family::cmdInitBatchBufferStart = Family::MI_BATCH_BUFFER_START::sInit();
const Family::MI_BATCH_BUFFER_END Family::cmdInitBatchBufferEnd = Family::MI_BATCH_BUFFER_END::sInit();
const Family::PIPE_CONTROL Family::cmdInitPipeControl = Family::PIPE_CONTROL::sInit();
const Family::RESOURCE_BARRIER Family::cmdInitResourceBarrier = Family::RESOURCE_BARRIER::sInit();
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
const Family::MI_MEM_FENCE Family::cmdInitMemFence = Family::MI_MEM_FENCE::sInit();
const Family::MEM_SET Family::cmdInitMemSet = Family::MEM_SET::sInit();
const Family::STATE_SIP Family::cmdInitStateSip = Family::STATE_SIP::sInit();
const Family::STATE_CONTEXT_DATA_BASE_ADDRESS Family::cmdInitStateContextDataBaseAddress = Family::STATE_CONTEXT_DATA_BASE_ADDRESS::sInit();
const Family::STATE_SYSTEM_MEM_FENCE_ADDRESS Family::cmdInitStateSystemMemFenceAddress = Family::STATE_SYSTEM_MEM_FENCE_ADDRESS::sInit();
} // namespace NEO
