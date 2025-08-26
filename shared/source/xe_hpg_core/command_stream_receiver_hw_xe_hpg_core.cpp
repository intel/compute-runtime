/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/source/xe_hpg_core/hw_info.h"

using Family = NEO::XeHpgCoreFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_dg2_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_heap_addressing.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_from_gen12lp_to_xe3.inl"
#include "shared/source/helpers/blit_commands_helper_xehp_and_later.inl"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/helpers/state_base_address_xehp_and_later.inl"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {
static auto gfxCore = IGFX_XE_HPG_CORE;

template <>
bool ImplicitFlushSettings<Family>::defaultSettingForNewResource = true;
template <>
bool ImplicitFlushSettings<Family>::defaultSettingForGpuIdle = false;
template class ImplicitFlushSettings<Family>;

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

template <>
MemoryCompressionState CommandStreamReceiverHw<Family>::getMemoryCompressionState(bool auxTranslationRequired) const {
    auto memoryCompressionState = MemoryCompressionState::notApplicable;

    if (CompressionSelector::allowStatelessCompression()) {
        memoryCompressionState = auxTranslationRequired ? MemoryCompressionState::disabled : MemoryCompressionState::enabled;
    }
    return memoryCompressionState;
}

template <>
void BlitCommandsHelper<Family>::adjustControlSurfaceType(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {
    using CONTROL_SURFACE_TYPE = typename Family::XY_BLOCK_COPY_BLT::CONTROL_SURFACE_TYPE;
    using COMPRESSION_ENABLE = typename Family::XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE;

    auto srcAllocation = blitProperties.srcAllocation;
    if (srcAllocation && srcAllocation->getDefaultGmm()) {
        auto gmmResourceInfo = srcAllocation->getDefaultGmm()->gmmResourceInfo.get();
        auto resInfo = gmmResourceInfo->getResourceFlags()->Info;
        if (resInfo.MediaCompressed) {
            blitCmd.setSourceControlSurfaceType(CONTROL_SURFACE_TYPE::CONTROL_SURFACE_TYPE_MEDIA);
        } else if (resInfo.RenderCompressed) {
            blitCmd.setSourceControlSurfaceType(CONTROL_SURFACE_TYPE::CONTROL_SURFACE_TYPE_3D);
        }
    }

    auto dstAllocation = blitProperties.dstAllocation;
    if (dstAllocation && dstAllocation->getDefaultGmm()) {
        auto gmmResourceInfo = dstAllocation->getDefaultGmm()->gmmResourceInfo.get();
        auto resInfo = gmmResourceInfo->getResourceFlags()->Info;
        if (resInfo.MediaCompressed) {
            blitCmd.setDestinationControlSurfaceType(CONTROL_SURFACE_TYPE::CONTROL_SURFACE_TYPE_MEDIA);
            blitCmd.setDestinationCompressionEnable(COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
        } else if (resInfo.RenderCompressed) {
            blitCmd.setDestinationControlSurfaceType(CONTROL_SURFACE_TYPE::CONTROL_SURFACE_TYPE_3D);
        }
    }
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;

    appendClearColor(blitProperties, blitCmd);

    uint32_t compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);
    if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
        compressionFormat = debugManager.flags.ForceBufferCompressionFormat.get();
    }

    auto compressionEnabledField = XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE;
    if (debugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.get() != -1) {
        compressionEnabledField = static_cast<typename XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE>(debugManager.flags.ForceCompressionDisabledForCompressedBlitCopies.get());
    }

    if (blitProperties.dstAllocation && blitProperties.dstAllocation->isCompressionEnabled()) {
        blitCmd.setDestinationCompressionEnable(compressionEnabledField);
        blitCmd.setDestinationAuxiliarysurfacemode(XY_BLOCK_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        blitCmd.setDestinationCompressionFormat(compressionFormat);
    }

    if (blitProperties.srcAllocation && blitProperties.srcAllocation->isCompressionEnabled()) {
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

    if (AuxTranslationDirection::auxToNonAux == blitProperties.auxTranslationDirection) {
        blitCmd.setSpecialModeofOperation(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE);
        UNRECOVERABLE_IF(blitCmd.getSourceTiling() != blitCmd.getDestinationTiling());
    } else if (AuxTranslationDirection::nonAuxToAux == blitProperties.auxTranslationDirection) {
        blitCmd.setSourceCompressionEnable(XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
    }

    DEBUG_BREAK_IF((AuxTranslationDirection::none != blitProperties.auxTranslationDirection) &&
                   (blitProperties.dstAllocation != blitProperties.srcAllocation || (blitProperties.dstAllocation && !blitProperties.dstAllocation->isCompressionEnabled())));

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getUncachedMOCS();

    if (debugManager.flags.OverrideBlitterMocs.get() != -1) {
        mocs = static_cast<uint32_t>(debugManager.flags.OverrideBlitterMocs.get());
    }

    blitCmd.setDestinationMOCS(mocs);
    blitCmd.setSourceMOCS(mocs);

    if (debugManager.flags.OverrideBlitterTargetMemory.get() != -1) {
        if (debugManager.flags.OverrideBlitterTargetMemory.get() == 0u) {
            blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
            blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        } else if (debugManager.flags.OverrideBlitterTargetMemory.get() == 1u) {
            blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
            blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
        }
    }
}

template <>
size_t BlitCommandsHelper<Family>::getNumberOfBlitsForByteFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    return NEO::BlitCommandsHelper<Family>::getNumberOfBlitsForFill(copySize, patternSize, rootDeviceEnvironment, isSystemMemoryPoolUsed);
}

template <>
BlitCommandsResult BlitCommandsHelper<Family>::dispatchBlitMemoryByteFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    return NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryFill(blitProperties, linearStream, rootDeviceEnvironment);
}

template <>
void BlitCommandsHelper<Family>::appendBlitMemSetCompressionFormat(void *blitCmd, NEO::GraphicsAllocation *dstAlloc, uint32_t compressionFormat) {}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;
template void BlitCommandsHelper<Family>::appendColorDepth<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd);
template void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);

template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last);

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
const Family::L3_CONTROL Family::cmdInitL3Control = Family::L3_CONTROL::sInit();
const Family::L3_FLUSH_ADDRESS_RANGE Family::cmdInitL3FlushAddressRange = Family::L3_FLUSH_ADDRESS_RANGE::sInit();
const Family::MI_FLUSH_DW Family::cmdInitMiFlushDw = Family::MI_FLUSH_DW::sInit();
const Family::XY_BLOCK_COPY_BLT Family::cmdInitXyBlockCopyBlt = Family::XY_BLOCK_COPY_BLT::sInit();
const Family::XY_BLOCK_COPY_BLT Family::cmdInitXyCopyBlt = Family::XY_BLOCK_COPY_BLT::sInit();
const Family::XY_FAST_COLOR_BLT Family::cmdInitXyColorBlt = Family::XY_FAST_COLOR_BLT::sInit();
const Family::_3DSTATE_BTD Family::cmd3dStateBtd = Family::_3DSTATE_BTD::sInit();
const Family::STATE_SIP Family::cmdInitStateSip = Family::STATE_SIP::sInit();
} // namespace NEO
