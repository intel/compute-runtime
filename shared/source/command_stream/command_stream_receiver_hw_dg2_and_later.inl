/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

using _3DSTATE_BTD = typename Family::_3DSTATE_BTD;
using _3DSTATE_BTD_BODY = typename Family::_3DSTATE_BTD_BODY;
using PIPE_CONTROL = typename Family::PIPE_CONTROL;

template <>
void CommandStreamReceiverHw<Family>::programPerDssBackedBuffer(LinearStream &commandStream, Device &device, DispatchFlags &dispatchFlags) {
    if (dispatchFlags.usePerDssBackedBuffer && !isPerDssBackedBufferSent) {
        DEBUG_BREAK_IF(perDssBackedBuffer == nullptr);

        auto cmd3dStateBtd = commandStream.getSpaceForCmd<_3DSTATE_BTD>();

        _3DSTATE_BTD cmd = Family::cmd3dStateBtd;
        cmd.getBtdStateBody().setPerDssMemoryBackedBufferSize(static_cast<_3DSTATE_BTD_BODY::PER_DSS_MEMORY_BACKED_BUFFER_SIZE>(RayTracingHelper::getMemoryBackedFifoSizeToPatch()));
        cmd.getBtdStateBody().setMemoryBackedBufferBasePointer(perDssBackedBuffer->getGpuAddress());
        *cmd3dStateBtd = cmd;
        isPerDssBackedBufferSent = true;
    }
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo) {
    size_t size = sizeof(_3DSTATE_BTD);

    auto &productHelper = getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs());
    std::ignore = isBasicWARequired;

    if (isExtendedWARequired) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier(false);
    }

    return size;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBefore3dState(LinearStream &commandStream, DispatchFlags &dispatchFlags) {
    if (!dispatchFlags.usePerDssBackedBuffer) {
        return;
    }

    if (isPerDssBackedBufferSent) {
        return;
    }

    auto &hwInfo = peekHwInfo();
    auto &productHelper = getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs());
    std::ignore = isBasicWARequired;

    PipeControlArgs args;
    args.dcFlushEnable = this->dcFlushSupport;

    if (isExtendedWARequired) {
        DEBUG_BREAK_IF(perDssBackedBuffer == nullptr);
        NEO::EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, this->peekRootDeviceEnvironment(), isRcs());
    }
}

} // namespace NEO
