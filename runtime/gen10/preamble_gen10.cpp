/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_stream/csr_definitions.h"
#include "runtime/helpers/preamble.inl"

namespace NEO {

template <>
uint32_t PreambleHelper<CNLFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.platform.eProductFamily) {
    case IGFX_CANNONLAKE:
        l3Config = getL3ConfigHelper<IGFX_CANNONLAKE>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_CANNONLAKE>(true);
    }
    return l3Config;
}

template <>
uint32_t PreambleHelper<CNLFamily>::getDefaultThreadArbitrationPolicy() {
    return ThreadArbitrationPolicy::RoundRobinAfterDependency;
}

template <>
void PreambleHelper<CNLFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
    UNRECOVERABLE_IF(requiredThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent);

    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = PIPE_CONTROL::sInit();
    pipeControl->setCommandStreamerStallEnable(true);

    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();

    pCmd->setRegisterOffset(RowChickenReg4::address);
    pCmd->setDataDword(RowChickenReg4::regDataForArbitrationPolicy[requiredThreadArbitrationPolicy]);
}

template <>
size_t PreambleHelper<CNLFamily>::getThreadArbitrationCommandsSize() {
    return sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
}

template <>
uint32_t PreambleHelper<CNLFamily>::getUrbEntryAllocationSize() {
    return 1024;
}

template <>
void PreambleHelper<CNLFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = PIPE_CONTROL::sInit();
    pipeControl->setCommandStreamerStallEnable(true);
    if (hwInfo->workaroundTable.waSendMIFLUSHBeforeVFE) {
        pipeControl->setRenderTargetCacheFlushEnable(true);
        pipeControl->setDepthCacheFlushEnable(true);
        pipeControl->setDcFlushEnable(true);
    }
}

template <>
void PreambleHelper<CNLFamily>::programPipelineSelect(LinearStream *pCommandStream, const DispatchFlags &dispatchFlags) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;

    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = PIPELINE_SELECT::sInit();

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    pCmd->setMaskBits(mask);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    pCmd->setMediaSamplerDopClockGateEnable(!dispatchFlags.mediaSamplerRequired);
}

template struct PreambleHelper<CNLFamily>;
} // namespace NEO
