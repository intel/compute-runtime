/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

using PIPE_CONTROL = typename Family::PIPE_CONTROL;

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo) {
    size_t size = EncodeEnableRayTracing<Family>::getCmdSizeFor3dStateBtd();
    const auto &releaseHelper = getReleaseHelper();
    const bool isExtendedWARequired = releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, isRcs());

    if (isExtendedWARequired) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier();
    }

    return size;
}

template <>
void CommandStreamReceiverHw<Family>::dispatchRayTracingStateCommand(LinearStream &cmdStream, Device &device) {
    auto &hwInfo = peekHwInfo();
    const auto &releaseHelper = getReleaseHelper();
    const bool isExtendedWARequired = releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, isRcs());

    if (isExtendedWARequired) {
        PipeControlArgs args;
        args.dcFlushEnable = this->dcFlushSupport;

        EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(cmdStream, args, this->peekRootDeviceEnvironment(), isRcs());
    }

    EncodeEnableRayTracing<Family>::programEnableRayTracing(cmdStream, device.getRTMemoryBackedBuffer()->getGpuAddress());
    this->setBtdCommandDirty(false);
}

} // namespace NEO
