/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble.h"

namespace NEO {

template <typename Family>
void PreambleHelper<Family>::programPipelineSelect(LinearStream *pCommandStream,
                                                   const PipelineSelectArgs &pipelineSelectArgs,
                                                   const RootDeviceEnvironment &rootDeviceEnvironment) {

    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;

    PIPELINE_SELECT cmd = Family::cmdInitPipelineSelect;

    if (MemorySynchronizationCommands<Family>::isBarrierPriorToPipelineSelectWaRequired(rootDeviceEnvironment)) {
        PipeControlArgs args;
        args.renderTargetCacheFlushEnable = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
    }

    if (debugManager.flags.CleanStateInPreamble.get()) {
        auto cmdBuffer = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
        cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_3D);
        *cmdBuffer = cmd;

        PipeControlArgs args = {};
        args.stateCacheInvalidationEnable = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
    }

    auto cmdBuffer = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();

    auto mask = pipelineSelectEnablePipelineSelectMaskBits;

    cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    if constexpr (Family::isUsingMediaSamplerDopClockGate) {
        mask |= pipelineSelectMediaSamplerDopClockGateMaskBits;
        cmd.setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);
    }

    bool systolicSupport = pipelineSelectArgs.systolicPipelineSelectSupport;
    bool systolicValue = pipelineSelectArgs.systolicPipelineSelectMode;
    int32_t overrideSystolic = debugManager.flags.OverrideSystolicPipelineSelect.get();

    if (overrideSystolic != -1) {
        systolicSupport = true;
        systolicValue = !!overrideSystolic;
    }

    if (systolicSupport) {
        cmd.setSystolicModeEnable(systolicValue);
        mask |= pipelineSelectSystolicModeEnableMaskBits;
    }

    cmd.setMaskBits(mask);

    *cmdBuffer = cmd;

    if (debugManager.flags.CleanStateInPreamble.get()) {
        PipeControlArgs args = {};
        args.stateCacheInvalidationEnable = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
    }
}

template <typename Family>
size_t PreambleHelper<Family>::getCmdSizeForPipelineSelect(const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t size = 0;
    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;
    size += sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<Family>::isBarrierPriorToPipelineSelectWaRequired(rootDeviceEnvironment)) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier();
    }
    if (debugManager.flags.CleanStateInPreamble.get()) {
        size += sizeof(PIPELINE_SELECT);
        size += 2 * MemorySynchronizationCommands<Family>::getSizeForSingleBarrier();
    }
    return size;
}

} // namespace NEO
