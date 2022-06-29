/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/source/xe_hpg_core/hw_info.h"

using Family = NEO::XE_HPG_COREFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_dg2_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xehp_and_later.inl"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {
static auto gfxCore = IGFX_XE_HPG_CORE;

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
MemoryCompressionState CommandStreamReceiverHw<Family>::getMemoryCompressionState(bool auxTranslationRequired, const HardwareInfo &hwInfo) const {
    auto memoryCompressionState = MemoryCompressionState::NotApplicable;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.allowStatelessCompression(hwInfo)) {
        memoryCompressionState = auxTranslationRequired ? MemoryCompressionState::Disabled : MemoryCompressionState::Enabled;
    }
    return memoryCompressionState;
}

template <>
void CommandStreamReceiverHw<Family>::programAdditionalStateBaseAddress(LinearStream &csr, typename Family::STATE_BASE_ADDRESS &cmd, Device &device) {
    using STATE_BASE_ADDRESS = Family::STATE_BASE_ADDRESS;

    auto &hwInfo = *device.getRootDeviceEnvironment().getHardwareInfo();
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(hwInfo)) {
        auto pCmd = static_cast<STATE_BASE_ADDRESS *>(csr.getSpace(sizeof(STATE_BASE_ADDRESS)));
        *pCmd = cmd;
    }
}

template class CommandStreamReceiverHw<Family>;
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
