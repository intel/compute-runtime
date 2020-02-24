/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw_bdw_plus.inl"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/blit_commands_helper_bdw_plus.inl"

#include "opencl/source/command_stream/device_command_stream.h"
#include "opencl/source/gen11/reg_configs.h"

namespace NEO {
typedef ICLFamily Family;
static auto gfxCore = IGFX_GEN11_CORE;

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForComputeMode() {
    if (csrSizeRequestFlags.coherencyRequestChanged) {
        return sizeof(typename Family::MI_LOAD_REGISTER_IMM);
    }
    return 0;
}

template <>
void CommandStreamReceiverHw<Family>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags) {
    if (csrSizeRequestFlags.coherencyRequestChanged) {
        LriHelper<Family>::program(&stream, gen11HdcModeRegister::address,
                                   DwordBuilder::build(gen11HdcModeRegister::forceNonCoherentEnableBit, true, !dispatchFlags.requiresCoherency));
        this->lastSentCoherencyRequest = static_cast<int8_t>(dispatchFlags.requiresCoherency);
    }
}

template <>
void CommandStreamReceiverHw<Family>::programMediaSampler(LinearStream &stream, DispatchFlags &dispatchFlags) {
    using PWR_CLK_STATE_REGISTER = Family::PWR_CLK_STATE_REGISTER;

    if (peekHwInfo().platform.eProductFamily == IGFX_ICELAKE_LP) {
        if (dispatchFlags.pipelineSelectArgs.mediaSamplerRequired) {
            if (!lastVmeSubslicesConfig) {
                auto pc = addPipeControlCmd(stream);
                pc->setDcFlushEnable(true);
                pc->setRenderTargetCacheFlushEnable(true);
                pc->setInstructionCacheInvalidateEnable(true);
                pc->setTextureCacheInvalidationEnable(true);
                pc->setPipeControlFlushEnable(true);
                pc->setVfCacheInvalidationEnable(true);
                pc->setConstantCacheInvalidationEnable(true);
                pc->setStateCacheInvalidationEnable(true);

                uint32_t numSubslices = peekHwInfo().gtSystemInfo.SubSliceCount;
                uint32_t numSubslicesWithVme = numSubslices / 2; // 1 VME unit per DSS
                uint32_t numSlicesForPowerGating = 1;            // power gating supported only if #Slices = 1

                PWR_CLK_STATE_REGISTER reg = Family::cmdInitPwrClkStateRegister;
                reg.TheStructure.Common.EUmin = peekHwInfo().gtSystemInfo.MaxEuPerSubSlice;
                reg.TheStructure.Common.EUmax = peekHwInfo().gtSystemInfo.MaxEuPerSubSlice;
                reg.TheStructure.Common.SSCountEn = 1; // Enable SScount
                reg.TheStructure.Common.SScount = numSubslicesWithVme;
                reg.TheStructure.Common.EnableSliceCountRequest = 1; // Enable SliceCountRequest
                reg.TheStructure.Common.SliceCountRequest = numSlicesForPowerGating;
                LriHelper<Family>::program(&stream, PWR_CLK_STATE_REGISTER::REG_ADDRESS, reg.TheStructure.RawData[0]);

                addPipeControlCmd(stream);

                lastVmeSubslicesConfig = true;
            }
        } else {
            if (lastVmeSubslicesConfig) {
                auto pc = addPipeControlCmd(stream);
                pc->setDcFlushEnable(true);
                pc->setRenderTargetCacheFlushEnable(true);
                pc->setInstructionCacheInvalidateEnable(true);
                pc->setTextureCacheInvalidationEnable(true);
                pc->setPipeControlFlushEnable(true);
                pc->setVfCacheInvalidationEnable(true);
                pc->setConstantCacheInvalidationEnable(true);
                pc->setStateCacheInvalidationEnable(true);
                pc->setGenericMediaStateClear(true);

                addPipeControlCmd(stream);

                // In Gen11-LP, software programs this register as if GT consists of
                // 2 slices with 4 subslices in each slice. Hardware maps this to the
                // LP 1 slice/8-subslice physical layout
                uint32_t numSubslices = peekHwInfo().gtSystemInfo.SubSliceCount;
                uint32_t numSubslicesMapped = numSubslices / 2;
                uint32_t numSlicesMapped = peekHwInfo().gtSystemInfo.SliceCount * 2;

                PWR_CLK_STATE_REGISTER reg = Family::cmdInitPwrClkStateRegister;
                reg.TheStructure.Common.EUmin = peekHwInfo().gtSystemInfo.MaxEuPerSubSlice;
                reg.TheStructure.Common.EUmax = peekHwInfo().gtSystemInfo.MaxEuPerSubSlice;
                reg.TheStructure.Common.SSCountEn = 1; // Enable SScount
                reg.TheStructure.Common.SScount = numSubslicesMapped;
                reg.TheStructure.Common.EnableSliceCountRequest = 1; // Enable SliceCountRequest
                reg.TheStructure.Common.SliceCountRequest = numSlicesMapped;

                LriHelper<Family>::program(&stream, PWR_CLK_STATE_REGISTER::REG_ADDRESS, reg.TheStructure.RawData[0]);

                addPipeControlCmd(stream);
            }
        }
    }
}

template <>
bool CommandStreamReceiverHw<Family>::detectInitProgrammingFlagsRequired(const DispatchFlags &dispatchFlags) const {
    bool flag = DebugManager.flags.ForceCsrReprogramming.get();
    if (peekHwInfo().platform.eProductFamily == IGFX_ICELAKE_LP) {
        if (!dispatchFlags.pipelineSelectArgs.mediaSamplerRequired) {
            if (lastVmeSubslicesConfig) {
                flag = true;
            }
        }
    }
    return flag;
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForMediaSampler(bool mediaSamplerRequired) const {
    typedef typename Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename Family::PIPE_CONTROL PIPE_CONTROL;

    if (peekHwInfo().platform.eProductFamily == IGFX_ICELAKE_LP) {
        if (mediaSamplerRequired) {
            if (!lastVmeSubslicesConfig) {
                return sizeof(MI_LOAD_REGISTER_IMM) + 2 * sizeof(PIPE_CONTROL);
            }
        } else {
            if (lastVmeSubslicesConfig) {
                return sizeof(MI_LOAD_REGISTER_IMM) + 3 * sizeof(PIPE_CONTROL);
            }
        }
    }
    return 0;
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
const Family::MI_SEMAPHORE_WAIT Family::cmdInitMiSemaphoreWait = Family::MI_SEMAPHORE_WAIT::sInit();
const Family::RENDER_SURFACE_STATE Family::cmdInitRenderSurfaceState = Family::RENDER_SURFACE_STATE::sInit();
const Family::PWR_CLK_STATE_REGISTER Family::cmdInitPwrClkStateRegister = Family::PWR_CLK_STATE_REGISTER::sInit();
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
