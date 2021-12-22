/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

using _3DSTATE_BTD = typename Family::_3DSTATE_BTD;
using _3DSTATE_BTD_BODY = typename Family::_3DSTATE_BTD_BODY;
using PIPE_CONTROL = typename Family::PIPE_CONTROL;

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::setPipeControlPriorToNonPipelinedStateCommandExtraProperties(PipeControlArgs &args) {
    args.unTypedDataPortCacheFlush = true;
}

template <>
void CommandStreamReceiverHw<Family>::programPerDssBackedBuffer(LinearStream &commandStream, Device &device, DispatchFlags &dispatchFlags) {
    if (dispatchFlags.usePerDssBackedBuffer && !isPerDssBackedBufferSent) {
        DEBUG_BREAK_IF(perDssBackedBuffer == nullptr);

        auto _3dStateBtd = commandStream.getSpaceForCmd<_3DSTATE_BTD>();

        _3DSTATE_BTD cmd = Family::cmd3dStateBtd;
        cmd.getBtdStateBody().setPerDssMemoryBackedBufferSize(static_cast<_3DSTATE_BTD_BODY::PER_DSS_MEMORY_BACKED_BUFFER_SIZE>(RayTracingHelper::getMemoryBackedFifoSizeToPatch()));
        cmd.getBtdStateBody().setMemoryBackedBufferBasePointer(perDssBackedBuffer->getGpuAddress());
        *_3dStateBtd = cmd;
        isPerDssBackedBufferSent = true;
    }
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo) {
    size_t size = sizeof(_3DSTATE_BTD);

    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs())) {
        size += sizeof(typename Family::PIPE_CONTROL);
    }

    return size;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBefore3dState(LinearStream &commandStream, DispatchFlags &dispatchFlags) {
    auto &hwInfo = peekHwInfo();
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);

    if (hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs()) && dispatchFlags.usePerDssBackedBuffer && !isPerDssBackedBufferSent) {
        DEBUG_BREAK_IF(perDssBackedBuffer == nullptr);

        addPipeControlPriorToNonPipelinedStateCommand(commandStream, args);
    }
}

} // namespace NEO
