/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/csr_definitions.h"
#include "core/helpers/preamble_bdw_plus.inl"
#include "runtime/gen12lp/helpers_gen12lp.h"
#include "runtime/helpers/hardware_commands_helper.h"

#include "reg_configs_common.h"

namespace NEO {

template <>
uint32_t PreambleHelper<TGLLPFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.platform.eProductFamily) {
    case IGFX_TIGERLAKE_LP:
        l3Config = getL3ConfigHelper<IGFX_TIGERLAKE_LP>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_TIGERLAKE_LP>(true);
    }
    return l3Config;
}

template <>
void PreambleHelper<TGLLPFamily>::programPipelineSelect(LinearStream *pCommandStream,
                                                        const PipelineSelectArgs &pipelineSelectArgs,
                                                        const HardwareInfo &hwInfo) {

    using PIPELINE_SELECT = typename TGLLPFamily::PIPELINE_SELECT;

    if (HardwareCommandsHelper<TGLLPFamily>::isPipeControlPriorToPipelineSelectWArequired(hwInfo)) {
        auto pipeControl = PipeControlHelper<TGLLPFamily>::addPipeControl(*pCommandStream, false);
        pipeControl->setRenderTargetCacheFlushEnable(true);
    }

    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = TGLLPFamily::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;

    pCmd->setMaskBits(mask);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    pCmd->setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);

    Gen12LPHelpers::setAdditionalPipelineSelectFields(pCmd, pipelineSelectArgs, hwInfo);
}

template <>
void PreambleHelper<TGLLPFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = TGLLPFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);
    if (hwInfo->workaroundTable.waSendMIFLUSHBeforeVFE) {
        if (hwInfo->capabilityTable.defaultEngineType != aub_stream::ENGINE_CCS) {
            pipeControl->setRenderTargetCacheFlushEnable(true);
            pipeControl->setDepthCacheFlushEnable(true);
        }
        pipeControl->setDcFlushEnable(true);
    }
}

template <>
void PreambleHelper<TGLLPFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
}

template <>
uint32_t PreambleHelper<TGLLPFamily>::getUrbEntryAllocationSize() {
    return 1024u;
}

template <>
void PreambleHelper<TGLLPFamily>::programAdditionalFieldsInVfeState(VFE_STATE_TYPE *mediaVfeState, const HardwareInfo &hwInfo) {
    mediaVfeState->setDisableSlice0Subslice2(hwInfo.workaroundTable.waDisableFusedThreadScheduling);

    if (DebugManager.flags.CFEFusedEUDispatch.get() != -1) {
        mediaVfeState->setDisableSlice0Subslice2(DebugManager.flags.CFEFusedEUDispatch.get());
    }
}
// Explicitly instantiate PreambleHelper for TGLLP device family

template struct PreambleHelper<TGLLPFamily>;
} // namespace NEO
