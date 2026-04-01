/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble.h"

namespace NEO {

template <typename Family>
size_t PreambleHelper<Family>::getCmdSizeForPipelineSelect(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.PipelinedPipelineSelect.get()) {
        return sizeof(typename Family::PIPELINE_SELECT);
    }

    return 0;
}

template <typename Family>
void PreambleHelper<Family>::programPipelineSelect(LinearStream *pCommandStream,
                                                   const PipelineSelectArgs &pipelineSelectArgs,
                                                   const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.PipelinedPipelineSelect.get()) {
        using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;

        auto cmdBuffer = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
        auto pipelineSelectCmd = Family::cmdInitPipelineSelect;

        auto mask = pipelineSelectEnablePipelineSelectMaskBits;
        pipelineSelectCmd.setMaskBits(mask);

        pipelineSelectCmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

        *cmdBuffer = pipelineSelectCmd;
    }
}

template <typename Family>
void PreambleHelper<Family>::appendProgramVFEState(const RootDeviceEnvironment &rootDeviceEnvironment, const StreamProperties &streamProperties, void *cmd) {
}
} // namespace NEO
