/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_info.h"
#include "hw_cmds.h"
#include "reg_configs_common.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/command_stream_receiver_hw.inl"

namespace OCLRT {
typedef CNLFamily Family;
static auto gfxCore = IGFX_GEN10_CORE;

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForComputeMode() {
    if (csrSizeRequestFlags.coherencyRequestChanged) {
        return sizeof(typename Family::MI_LOAD_REGISTER_IMM);
    }
    return 0;
}

template <>
void CommandStreamReceiverHw<Family>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags) {
    if (csrSizeRequestFlags.coherencyRequestChanged) {
        LriHelper<Family>::program(&stream, gen10HdcModeRegisterAddresss, DwordBuilder::build(4, true, !dispatchFlags.requiresCoherency));
        this->lastSentCoherencyRequest = static_cast<int8_t>(dispatchFlags.requiresCoherency);
    }
}

template <>
void CommandStreamReceiverHw<Family>::addPipeControlWA(LinearStream &commandStream, bool flushDC) {
    auto pCmd = (Family::PIPE_CONTROL *)commandStream.getSpace(sizeof(Family::PIPE_CONTROL));
    *pCmd = Family::cmdInitPipeControl;

    pCmd->setDcFlushEnable(flushDC);
    pCmd->setCommandStreamerStallEnable(true);
}

template <>
int CommandStreamReceiverHw<Family>::getRequiredPipeControlSize() const {
    return 2 * sizeof(Family::PIPE_CONTROL);
}

template <>
void CommandStreamReceiverHw<Family>::addDcFlushToPipeControl(Family::PIPE_CONTROL *pCmd, bool flushDC) {
}

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

// Explicitly instantiate CommandStreamReceiverHw for this device family
template class CommandStreamReceiverHw<Family>;

const Family::GPGPU_WALKER Family::cmdInitGpgpuWalker = Family::GPGPU_WALKER::sInit();
const Family::INTERFACE_DESCRIPTOR_DATA Family::cmdInitInterfaceDescriptorData = Family::INTERFACE_DESCRIPTOR_DATA::sInit();
const Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD Family::cmdInitMediaInterfaceDescriptorLoad = Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD::sInit();
const Family::MEDIA_STATE_FLUSH Family::cmdInitMediaStateFlush = Family::MEDIA_STATE_FLUSH::sInit();
const Family::MI_BATCH_BUFFER_START Family::cmdInitBatchBufferStart = Family::MI_BATCH_BUFFER_START::sInit();
const Family::MI_BATCH_BUFFER_END Family::cmdInitBatchBufferEnd = Family::MI_BATCH_BUFFER_END::sInit();
const Family::PIPE_CONTROL Family::cmdInitPipeControl = Family::PIPE_CONTROL::sInit();
const Family::MI_SEMAPHORE_WAIT Family::cmdInitMiSemaphoreWait = Family::MI_SEMAPHORE_WAIT::sInit();
const Family::RENDER_SURFACE_STATE Family::cmdRenderSurfaceState = Family::RENDER_SURFACE_STATE::sInit();
} // namespace OCLRT
