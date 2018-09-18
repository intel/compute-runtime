/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/preamble.inl"

namespace OCLRT {

template <>
uint32_t PreambleHelper<SKLFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.pPlatform->eProductFamily) {
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
void PreambleHelper<SKLFamily>::programPipelineSelect(LinearStream *pCommandStream, bool mediaSamplerRequired) {
    typedef typename SKLFamily::PIPELINE_SELECT PIPELINE_SELECT;

    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = PIPELINE_SELECT::sInit();

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    pCmd->setMaskBits(mask);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    pCmd->setMediaSamplerDopClockGateEnable(!mediaSamplerRequired);
}

template <>
void PreambleHelper<SKLFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = PIPE_CONTROL::sInit();
    pipeControl->setCommandStreamerStallEnable(true);
    if (hwInfo->pWaTable->waSendMIFLUSHBeforeVFE) {
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
    *pipeControl = PIPE_CONTROL::sInit();
    pipeControl->setCommandStreamerStallEnable(true);

    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();

    pCmd->setRegisterOffset(DebugControlReg2::address);
    pCmd->setDataDword(DebugControlReg2::getRegData(requiredThreadArbitrationPolicy));
}

template <>
size_t PreambleHelper<SKLFamily>::getThreadArbitrationCommandsSize() {
    return sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
}

template <>
size_t PreambleHelper<SKLFamily>::getAdditionalCommandsSize(const Device &device) {
    size_t totalSize = PreemptionHelper::getRequiredPreambleSize<SKLFamily>(device);
    totalSize += getKernelDebuggingCommandsSize(device.isSourceLevelDebuggerActive());
    return totalSize;
}

template struct PreambleHelper<SKLFamily>;
} // namespace OCLRT
