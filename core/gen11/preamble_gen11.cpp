/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/pipeline_select_helper.h"
#include "core/helpers/preamble_bdw_plus.inl"
#include "runtime/command_stream/csr_definitions.h"

#include "reg_configs_common.h"

namespace NEO {

template <>
uint32_t PreambleHelper<ICLFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.platform.eProductFamily) {
    case IGFX_ICELAKE_LP:
        l3Config = getL3ConfigHelper<IGFX_ICELAKE_LP>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_ICELAKE_LP>(true);
    }
    return l3Config;
}

template <>
void PreambleHelper<ICLFamily>::programPipelineSelect(LinearStream *pCommandStream,
                                                      const PipelineSelectArgs &pipelineSelectArgs,
                                                      const HardwareInfo &hwInfo) {

    typedef typename ICLFamily::PIPELINE_SELECT PIPELINE_SELECT;

    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = ICLFamily::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits |
                pipelineSelectMediaSamplerDopClockGateMaskBits |
                pipelineSelectMediaSamplerPowerClockGateMaskBits;

    pCmd->setMaskBits(mask);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    pCmd->setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);
    pCmd->setMediaSamplerPowerClockGateDisable(pipelineSelectArgs.mediaSamplerRequired);
}

template <>
void PreambleHelper<ICLFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = ICLFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);
    if (hwInfo->workaroundTable.waSendMIFLUSHBeforeVFE) {
        pipeControl->setRenderTargetCacheFlushEnable(true);
        pipeControl->setDepthCacheFlushEnable(true);
        pipeControl->setDcFlushEnable(true);
    }
}

template <>
uint32_t PreambleHelper<ICLFamily>::getDefaultThreadArbitrationPolicy() {
    return ThreadArbitrationPolicy::RoundRobinAfterDependency;
}

template <>
void PreambleHelper<ICLFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
    UNRECOVERABLE_IF(requiredThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent);

    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = ICLFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);

    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *pCmd = ICLFamily::cmdInitLoadRegisterImm;

    pCmd->setRegisterOffset(RowChickenReg4::address);
    pCmd->setDataDword(RowChickenReg4::regDataForArbitrationPolicy[requiredThreadArbitrationPolicy]);
}

template <>
size_t PreambleHelper<ICLFamily>::getThreadArbitrationCommandsSize() {
    return sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
}

template <>
size_t PreambleHelper<ICLFamily>::getAdditionalCommandsSize(const Device &device) {
    size_t totalSize = PreemptionHelper::getRequiredPreambleSize<ICLFamily>(device);
    totalSize += getKernelDebuggingCommandsSize(device.isSourceLevelDebuggerActive());
    return totalSize;
}

template struct PreambleHelper<ICLFamily>;
} // namespace NEO
