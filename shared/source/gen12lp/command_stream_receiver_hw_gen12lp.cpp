/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

using Family = NEO::TGLLPFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_bdw_plus.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_tgllp_plus.inl"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/helpers/blit_commands_helper_bdw_plus.inl"

namespace NEO {
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
void CommandStreamReceiverHw<Family>::programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config) {
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForL3Config() const {
    return 0;
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForComputeMode() {
    if (!csrSizeRequestFlags.hasSharedHandles) {
        for (const auto &allocation : this->getResidencyAllocations()) {
            if (allocation->peekSharedHandle()) {
                csrSizeRequestFlags.hasSharedHandles = true;
                break;
            }
        }
    }

    size_t size = 0;
    if (csrSizeRequestFlags.coherencyRequestChanged || csrSizeRequestFlags.hasSharedHandles || csrSizeRequestFlags.numGrfRequiredChanged) {
        size += sizeof(typename Family::STATE_COMPUTE_MODE);
        if (csrSizeRequestFlags.hasSharedHandles) {
            size += sizeof(typename Family::PIPE_CONTROL);
        }
    }
    return size;
}

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];
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
const Family::STATE_COMPUTE_MODE Family::cmdInitStateComputeMode = Family::STATE_COMPUTE_MODE::sInit();
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
