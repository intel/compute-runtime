/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/source/xe_hp_core/hw_info.h"

using Family = NEO::XeHpFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xehp_and_later.inl"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/helpers/state_base_address_xehp_and_later.inl"

namespace NEO {

static auto gfxCore = IGFX_XE_HP_CORE;

template <>
bool ImplicitFlushSettings<Family>::defaultSettingForNewResource = true;
template <>
bool ImplicitFlushSettings<Family>::defaultSettingForGpuIdle = true;
template class ImplicitFlushSettings<Family>;

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

template <>
MemoryCompressionState CommandStreamReceiverHw<Family>::getMemoryCompressionState(bool auxTranslationRequired, const HardwareInfo &hwInfo) const {
    auto memoryCompressionState = MemoryCompressionState::NotApplicable;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.allowStatelessCompression(hwInfo)) {
        memoryCompressionState = auxTranslationRequired ? MemoryCompressionState::Disabled : MemoryCompressionState::Enabled;
    }
    return memoryCompressionState;
}

template <>
GraphicsAllocation *CommandStreamReceiverHw<Family>::getClearColorAllocation() {
    constexpr uint32_t clearColorSize = 16u;
    static uint8_t clearColorBuffer[clearColorSize];
    if (DebugManager.flags.UseClearColorAllocationForBlitter.get()) {
        if (clearColorAllocation == nullptr) {
            auto lock = this->obtainUniqueOwnership();
            if (clearColorAllocation == nullptr) {
                AllocationProperties properties{rootDeviceIndex, clearColorSize, AllocationType::BUFFER, osContext->getDeviceBitfield()};
                properties.flags.readOnlyMultiStorage = true;
                properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = false;
                clearColorAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(properties, clearColorBuffer);
            }
        }
    }
    return clearColorAllocation;
}

template <>
void CommandStreamReceiverHw<Family>::programPerDssBackedBuffer(LinearStream &commandStream, Device &device, DispatchFlags &dispatchFlags) {
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo) {
    return 0;
}

template <>
void CommandStreamReceiverHw<Family>::addPipeControlBefore3dState(LinearStream &commandStream, DispatchFlags &dispatchFlags) {}

template <>
constexpr bool CommandStreamReceiverHw<Family>::isGlobalAtomicsProgrammingRequired(bool currentValue) const {
    return currentValue != this->lastSentUseGlobalAtomics;
}

template <>
void BlitCommandsHelper<Family>::appendClearColor(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;
    if (DebugManager.flags.UseClearColorAllocationForBlitter.get()) {
        blitCmd.setSourceClearValueEnable(XY_BLOCK_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_ENABLE);
        blitCmd.setDestinationClearValueEnable(XY_BLOCK_COPY_BLT::CLEAR_VALUE_ENABLE::CLEAR_VALUE_ENABLE_ENABLE);
        auto clearColorAddress = blitProperties.clearColorAllocation->getGpuAddress();
        blitCmd.setSourceClearAddressLow(static_cast<uint32_t>(clearColorAddress & 0xFFFFFFFFULL));
        blitCmd.setSourceClearAddressHigh(static_cast<uint32_t>(clearColorAddress >> 32));
        blitCmd.setDestinationClearAddressLow(static_cast<uint32_t>(clearColorAddress & 0xFFFFFFFFULL));
        blitCmd.setDestinationClearAddressHigh(static_cast<uint32_t>(clearColorAddress >> 32));
    }
}

template class CommandStreamReceiverHw<Family>;

template <>
void BlitCommandsHelper<Family>::appendExtraMemoryProperties(typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;

    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto &hwHelper = HwHelperHw<Family>::get();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);

    if (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo) &&
        hwInfoConfig.getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed) {
        blitCmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
        blitCmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    }
}

template <>
void BlitCommandsHelper<Family>::appendExtraMemoryProperties(typename Family::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using XY_COLOR_BLT = typename Family::XY_COLOR_BLT;

    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto &hwHelper = HwHelperHw<Family>::get();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);

    if (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo) &&
        hwInfoConfig.getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed) {
        blitCmd.setDestinationTargetMemory(XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
    }
}

template struct BlitCommandsHelper<Family>;
template void BlitCommandsHelper<Family>::appendColorDepth<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd);
template void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);

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
const Family::_3DSTATE_BTD_BODY Family::cmd3dStateBtdBody = Family::_3DSTATE_BTD_BODY::sInit();
const Family::STATE_SIP Family::cmdInitStateSip = Family::STATE_SIP::sInit();
} // namespace NEO
