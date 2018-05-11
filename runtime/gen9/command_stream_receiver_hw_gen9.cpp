/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "hw_info.h"
#include "hw_cmds.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/command_stream_receiver_hw.inl"

namespace OCLRT {
typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForCoherency() {
    return 0;
}

template <>
void CommandStreamReceiverHw<Family>::programCoherency(LinearStream &stream, DispatchFlags &dispatchFlags) {
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
} // namespace OCLRT
