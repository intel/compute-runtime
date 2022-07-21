/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble_bdw_and_later.inl"

#include "reg_configs_common.h"

namespace NEO {

using Family = ICLFamily;

template <>
uint32_t PreambleHelper<Family>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
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
void PreambleHelper<Family>::programPipelineSelect(LinearStream *pCommandStream,
                                                   const PipelineSelectArgs &pipelineSelectArgs,
                                                   const HardwareInfo &hwInfo) {

    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;

    auto pCmd = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
    PIPELINE_SELECT cmd = Family::cmdInitPipelineSelect;

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
void PreambleHelper<Family>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
    PipeControlArgs args = {};
    if (hwInfo->workaroundTable.flags.waSendMIFLUSHBeforeVFE) {
        args.renderTargetCacheFlushEnable = true;
        args.depthCacheFlushEnable = true;
        args.dcFlushEnable = true;
    }
    MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
}

template <>
std::vector<int32_t> PreambleHelper<Family>::getSupportedThreadArbitrationPolicies() {
    std::vector<int32_t> retVal;
    int32_t policySize = sizeof(RowChickenReg4::regDataForArbitrationPolicy) /
                         sizeof(RowChickenReg4::regDataForArbitrationPolicy[0]);
    for (int32_t i = 0; i < policySize; i++) {
        retVal.push_back(i);
    }
    return retVal;
}
template <>
size_t PreambleHelper<Family>::getAdditionalCommandsSize(const Device &device) {
    size_t totalSize = PreemptionHelper::getRequiredPreambleSize<Family>(device);
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();
    totalSize += getKernelDebuggingCommandsSize(debuggingEnabled);
    return totalSize;
}

template struct PreambleHelper<Family>;
} // namespace NEO
