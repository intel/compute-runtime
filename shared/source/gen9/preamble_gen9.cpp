/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/preamble_bdw_plus.inl"

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

    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = SKLFamily::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    pCmd->setMaskBits(mask);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    pCmd->setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);
}

template <>
void PreambleHelper<SKLFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, aub_stream::EngineType engineType) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = SKLFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);
    if (hwInfo->workaroundTable.waSendMIFLUSHBeforeVFE) {
        pipeControl->setRenderTargetCacheFlushEnable(true);
        pipeControl->setDepthCacheFlushEnable(true);
        pipeControl->setDcFlushEnable(true);
    }
}

template <>
uint32_t PreambleHelper<SKLFamily>::getDefaultThreadArbitrationPolicy() {
    return ThreadArbitrationPolicy::RoundRobin;
}

template <>
void PreambleHelper<SKLFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
    UNRECOVERABLE_IF(requiredThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent);

    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = SKLFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);

    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *pCmd = SKLFamily::cmdInitLoadRegisterImm;

    pCmd->setRegisterOffset(DebugControlReg2::address);
    pCmd->setDataDword(DebugControlReg2::getRegData(requiredThreadArbitrationPolicy));
}

template <>
size_t PreambleHelper<SKLFamily>::getThreadArbitrationCommandsSize() {
    return sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
}

template struct PreambleHelper<SKLFamily>;
} // namespace NEO
