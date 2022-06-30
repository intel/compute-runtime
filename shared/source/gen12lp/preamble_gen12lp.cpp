/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble_bdw_and_later.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

using Family = TGLLPFamily;

template <>
uint32_t PreambleHelper<Family>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
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
void PreambleHelper<Family>::programPipelineSelect(LinearStream *pCommandStream,
                                                   const PipelineSelectArgs &pipelineSelectArgs,
                                                   const HardwareInfo &hwInfo) {

    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;

    if (MemorySynchronizationCommands<Family>::isPipeControlPriorToPipelineSelectWArequired(hwInfo)) {
        PipeControlArgs args;
        args.renderTargetCacheFlushEnable = true;
        MemorySynchronizationCommands<Family>::addPipeControl(*pCommandStream, args);
    }

    auto pCmd = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
    PIPELINE_SELECT cmd = Family::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto pipeline = pipelineSelectArgs.is3DPipelineRequired ? PIPELINE_SELECT::PIPELINE_SELECTION_3D : PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;

    cmd.setMaskBits(mask);
    cmd.setPipelineSelection(pipeline);
    cmd.setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);

    HwInfoConfig::get(hwInfo.platform.eProductFamily)->setAdditionalPipelineSelectFields(&cmd, pipelineSelectArgs, hwInfo);

    *pCmd = cmd;
}

template <>
void PreambleHelper<Family>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
    PipeControlArgs args = {};
    if (hwInfo->workaroundTable.flags.waSendMIFLUSHBeforeVFE) {
        if (engineGroupType != EngineGroupType::Compute) {
            args.renderTargetCacheFlushEnable = true;
            args.depthCacheFlushEnable = true;
            args.depthStallEnable = true;
        }
        args.dcFlushEnable = true;
    }

    MemorySynchronizationCommands<Family>::addPipeControl(*pCommandStream, args);
}

template <>
void PreambleHelper<Family>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
}

template <>
uint32_t PreambleHelper<Family>::getUrbEntryAllocationSize() {
    return 1024u;
}

template <>
void PreambleHelper<Family>::programAdditionalFieldsInVfeState(VFE_STATE_TYPE *mediaVfeState, const HardwareInfo &hwInfo, bool disableEUFusion) {
    auto &hwHelper = HwHelperHw<Family>::get();
    if (!hwHelper.isFusedEuDispatchEnabled(hwInfo, disableEUFusion)) {
        mediaVfeState->setDisableSlice0Subslice2(true);
    }
    if (DebugManager.flags.MediaVfeStateMaxSubSlices.get() != -1) {
        mediaVfeState->setMaximumNumberOfDualSubslices(DebugManager.flags.MediaVfeStateMaxSubSlices.get());
    }
}

// Explicitly instantiate PreambleHelper for TGLLP device family
template struct PreambleHelper<Family>;
} // namespace NEO
