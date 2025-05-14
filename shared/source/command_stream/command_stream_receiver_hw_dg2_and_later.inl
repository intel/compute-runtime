/*
 * Copyright (C) 2021-2025 Intel Corporation
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
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

using _3DSTATE_BTD = typename Family::_3DSTATE_BTD;
using PIPE_CONTROL = typename Family::PIPE_CONTROL;

template <>
void CommandStreamReceiverHw<Family>::programPerDssBackedBuffer(LinearStream &commandStream, Device &device, DispatchFlags &dispatchFlags) {
    if (dispatchFlags.usePerDssBackedBuffer && !isPerDssBackedBufferSent) {
        DEBUG_BREAK_IF(perDssBackedBuffer == nullptr);
        EncodeEnableRayTracing<Family>::programEnableRayTracing(commandStream, perDssBackedBuffer->getGpuAddress());
        isPerDssBackedBufferSent = true;
    }
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo) {
    size_t size = sizeof(_3DSTATE_BTD);
    auto *releaseHelper = getReleaseHelper();
    auto &productHelper = getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs(), releaseHelper);
    std::ignore = isBasicWARequired;

    if (isExtendedWARequired) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier();
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
    auto *releaseHelper = getReleaseHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs(), releaseHelper);
    std::ignore = isBasicWARequired;

    PipeControlArgs args;
    args.dcFlushEnable = this->dcFlushSupport;

    if (isExtendedWARequired) {
        DEBUG_BREAK_IF(perDssBackedBuffer == nullptr);
        NEO::EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, this->peekRootDeviceEnvironment(), isRcs());
    }
}

template <>
void CommandStreamReceiverHw<Family>::dispatchRayTracingStateCommand(LinearStream &cmdStream, Device &device) {
    auto &hwInfo = peekHwInfo();
    auto &productHelper = getProductHelper();
    auto *releaseHelper = getReleaseHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs(), releaseHelper);
    std::ignore = isBasicWARequired;

    if (isExtendedWARequired) {
        PipeControlArgs args;
        args.dcFlushEnable = this->dcFlushSupport;

        EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(cmdStream, args, this->peekRootDeviceEnvironment(), isRcs());
    }

    EncodeEnableRayTracing<Family>::programEnableRayTracing(cmdStream, device.getRTMemoryBackedBuffer()->getGpuAddress());
    this->setBtdCommandDirty(false);
}

} // namespace NEO
