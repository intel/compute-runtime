/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/preamble_bdw_and_later.inl"

namespace NEO {

template <>
void PreambleHelper<BDWFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = BDWFamily::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);
    cmd.setDcFlushEnable(true);
    *pipeControl = cmd;
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

    using PIPELINE_SELECT = typename BDWFamily::PIPELINE_SELECT;
    auto pCmd = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
    PIPELINE_SELECT cmd = BDWFamily::cmdInitPipelineSelect;

    cmd.setMaskBits(pipelineSelectEnablePipelineSelectMaskBits);
    cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

    *pCmd = cmd;
}

template <>
size_t PreambleHelper<BDWFamily>::getAdditionalCommandsSize(const Device &device) {
    return getKernelDebuggingCommandsSize(device.isDebuggerActive());
}

template struct PreambleHelper<BDWFamily>;
} // namespace NEO
