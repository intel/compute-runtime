/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/preamble.inl"

namespace OCLRT {

template <>
void PreambleHelper<BDWFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = PIPE_CONTROL::sInit();
    pipeControl->setCommandStreamerStallEnable(true);
    pipeControl->setDcFlushEnable(true);
}

template <>
uint32_t PreambleHelper<BDWFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.pPlatform->eProductFamily) {
    case IGFX_BROADWELL:
        l3Config = getL3ConfigHelper<IGFX_BROADWELL>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_BROADWELL>(true);
    }
    return l3Config;
}

template <>
void PreambleHelper<BDWFamily>::programPipelineSelect(LinearStream *pCommandStream, const DispatchFlags &dispatchFlags) {
    typedef typename BDWFamily::PIPELINE_SELECT PIPELINE_SELECT;
    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = PIPELINE_SELECT::sInit();
    pCmd->setMaskBits(pipelineSelectEnablePipelineSelectMaskBits);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
}

template <>
size_t PreambleHelper<BDWFamily>::getAdditionalCommandsSize(const Device &device) {
    return getKernelDebuggingCommandsSize(device.isSourceLevelDebuggerActive());
}

template struct PreambleHelper<BDWFamily>;
} // namespace OCLRT
