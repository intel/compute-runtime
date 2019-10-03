/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/preamble_bdw_plus.inl"

namespace NEO {

template <>
void PreambleHelper<BDWFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = BDWFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);
    pipeControl->setDcFlushEnable(true);
}

template <>
uint32_t PreambleHelper<BDWFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.platform.eProductFamily) {
    case IGFX_BROADWELL:
        l3Config = getL3ConfigHelper<IGFX_BROADWELL>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_BROADWELL>(true);
    }
    return l3Config;
}

template <>
bool PreambleHelper<BDWFamily>::isL3Configurable(const HardwareInfo &hwInfo) {
    return getL3Config(hwInfo, true) != getL3Config(hwInfo, false);
}

template <>
void PreambleHelper<BDWFamily>::programPipelineSelect(LinearStream *pCommandStream,
                                                      const PipelineSelectArgs &pipelineSelectArgs,
                                                      const HardwareInfo &hwInfo) {

    typedef typename BDWFamily::PIPELINE_SELECT PIPELINE_SELECT;
    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = BDWFamily::cmdInitPipelineSelect;
    pCmd->setMaskBits(pipelineSelectEnablePipelineSelectMaskBits);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
}

template <>
size_t PreambleHelper<BDWFamily>::getAdditionalCommandsSize(const Device &device) {
    return getKernelDebuggingCommandsSize(device.isSourceLevelDebuggerActive());
}

template struct PreambleHelper<BDWFamily>;
} // namespace NEO
