/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble_bdw_plus.inl"

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

    using PIPELINE_SELECT = typename ICLFamily::PIPELINE_SELECT;

    auto pCmd = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
    PIPELINE_SELECT cmd = ICLFamily::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits |
                pipelineSelectMediaSamplerDopClockGateMaskBits |
                pipelineSelectMediaSamplerPowerClockGateMaskBits;

    cmd.setMaskBits(mask);
    cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    cmd.setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);
    cmd.setMediaSamplerPowerClockGateDisable(pipelineSelectArgs.mediaSamplerRequired);

    *pCmd = cmd;
}

template <>
void PreambleHelper<ICLFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, aub_stream::EngineType engineType) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = ICLFamily::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);

    if (hwInfo->workaroundTable.waSendMIFLUSHBeforeVFE) {
        cmd.setRenderTargetCacheFlushEnable(true);
        cmd.setDepthCacheFlushEnable(true);
        cmd.setDcFlushEnable(true);
    }
    *pipeControl = cmd;
}

template <>
void PreambleHelper<ICLFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
    UNRECOVERABLE_IF(requiredThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent);

    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = ICLFamily::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);
    *pipeControl = cmd;

    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    MI_LOAD_REGISTER_IMM lriCmd = ICLFamily::cmdInitLoadRegisterImm;

    lriCmd.setRegisterOffset(RowChickenReg4::address);
    lriCmd.setDataDword(RowChickenReg4::regDataForArbitrationPolicy[requiredThreadArbitrationPolicy]);

    *pCmd = lriCmd;
}

template <>
size_t PreambleHelper<ICLFamily>::getThreadArbitrationCommandsSize() {
    return sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
}

template <>
size_t PreambleHelper<ICLFamily>::getAdditionalCommandsSize(const Device &device) {
    size_t totalSize = PreemptionHelper::getRequiredPreambleSize<ICLFamily>(device);
    totalSize += getKernelDebuggingCommandsSize(device.isDebuggerActive());
    return totalSize;
}

template struct PreambleHelper<ICLFamily>;
} // namespace NEO
