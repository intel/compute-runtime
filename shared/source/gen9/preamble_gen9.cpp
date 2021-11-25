/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/preamble_bdw_and_later.inl"

namespace NEO {

template <>
uint32_t PreambleHelper<SKLFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.platform.eProductFamily) {
    case IGFX_SKYLAKE:
        l3Config = getL3ConfigHelper<IGFX_SKYLAKE>(useSLM);
        break;
    case IGFX_BROXTON:
        l3Config = getL3ConfigHelper<IGFX_BROXTON>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_SKYLAKE>(true);
    }
    return l3Config;
}

template <>
bool PreambleHelper<SKLFamily>::isL3Configurable(const HardwareInfo &hwInfo) {
    return getL3Config(hwInfo, true) != getL3Config(hwInfo, false);
}

template <>
void PreambleHelper<SKLFamily>::programPipelineSelect(LinearStream *pCommandStream,
                                                      const PipelineSelectArgs &pipelineSelectArgs,
                                                      const HardwareInfo &hwInfo) {

    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;

    auto pCmd = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
    PIPELINE_SELECT cmd = SKLFamily::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    cmd.setMaskBits(mask);
    cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    cmd.setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);

    *pCmd = cmd;
}

template <>
void PreambleHelper<SKLFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = SKLFamily::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);
    if (hwInfo->workaroundTable.flags.waSendMIFLUSHBeforeVFE) {
        cmd.setRenderTargetCacheFlushEnable(true);
        cmd.setDepthCacheFlushEnable(true);
        cmd.setDcFlushEnable(true);
    }
    *pipeControl = cmd;
}

template <>
void PreambleHelper<SKLFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
    UNRECOVERABLE_IF(requiredThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent);

    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = SKLFamily::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);
    *pipeControl = cmd;

    LriHelper<SKLFamily>::program(pCommandStream,
                                  DebugControlReg2::address,
                                  DebugControlReg2::getRegData(requiredThreadArbitrationPolicy),
                                  false);
}

template <>
size_t PreambleHelper<SKLFamily>::getThreadArbitrationCommandsSize() {
    return sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
}

template <>
std::vector<uint32_t> PreambleHelper<SKLFamily>::getSupportedThreadArbitrationPolicies() {
    std::vector<uint32_t> retVal;
    for (const uint32_t &p : DebugControlReg2::supportedArbitrationPolicy) {
        retVal.push_back(p);
    }
    return retVal;
}
template struct PreambleHelper<SKLFamily>;
} // namespace NEO
