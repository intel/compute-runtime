/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.inl"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/helpers/blit_commands_helper.inl"

#include "hw_cmds.h"
#include "hw_info.h"

namespace NEO {
typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForComputeMode() {
    return 0;
}

template <>
void CommandStreamReceiverHw<Family>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags) {
}

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;

const Family::GPGPU_WALKER Family::cmdInitGpgpuWalker = Family::GPGPU_WALKER::sInit();
const Family::INTERFACE_DESCRIPTOR_DATA Family::cmdInitInterfaceDescriptorData = Family::INTERFACE_DESCRIPTOR_DATA::sInit();
const Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD Family::cmdInitMediaInterfaceDescriptorLoad = Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD::sInit();
const Family::MEDIA_STATE_FLUSH Family::cmdInitMediaStateFlush = Family::MEDIA_STATE_FLUSH::sInit();
const Family::MI_BATCH_BUFFER_START Family::cmdInitBatchBufferStart = Family::MI_BATCH_BUFFER_START::sInit();
const Family::MI_BATCH_BUFFER_END Family::cmdInitBatchBufferEnd = Family::MI_BATCH_BUFFER_END::sInit();
const Family::PIPE_CONTROL Family::cmdInitPipeControl = Family::PIPE_CONTROL::sInit();
const Family::MI_SEMAPHORE_WAIT Family::cmdInitMiSemaphoreWait = Family::MI_SEMAPHORE_WAIT::sInit();
const Family::RENDER_SURFACE_STATE Family::cmdInitRenderSurfaceState = Family::RENDER_SURFACE_STATE::sInit();
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
const Family::MEDIA_VFE_STATE Family::cmdInitMediaVfeState = Family::MEDIA_VFE_STATE::sInit();
const Family::STATE_BASE_ADDRESS Family::cmdInitStateBaseAddress = Family::STATE_BASE_ADDRESS::sInit();
const Family::MEDIA_SURFACE_STATE Family::cmdInitMediaSurfaceState = Family::MEDIA_SURFACE_STATE::sInit();
const Family::SAMPLER_STATE Family::cmdInitSamplerState = Family::SAMPLER_STATE::sInit();
const Family::GPGPU_CSR_BASE_ADDRESS Family::cmdInitGpgpuCsrBaseAddress = Family::GPGPU_CSR_BASE_ADDRESS::sInit();
const Family::STATE_SIP Family::cmdInitStateSip = Family::STATE_SIP::sInit();
const Family::BINDING_TABLE_STATE Family::cmdInitBindingTableState = Family::BINDING_TABLE_STATE::sInit();
const Family::MI_USER_INTERRUPT Family::cmdInitUserInterrupt = Family::MI_USER_INTERRUPT::sInit();
const Family::XY_SRC_COPY_BLT Family::cmdInitXyCopyBlt = Family::XY_SRC_COPY_BLT::sInit();
const Family::MI_FLUSH_DW Family::cmdInitMiFlushDw = Family::MI_FLUSH_DW::sInit();
} // namespace NEO
