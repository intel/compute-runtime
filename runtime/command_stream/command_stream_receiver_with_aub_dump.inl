/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"

namespace OCLRT {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <typename BaseCSR>
CommandStreamReceiverWithAUBDump<BaseCSR>::CommandStreamReceiverWithAUBDump(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment)
    : BaseCSR(hwInfoIn, executionEnvironment) {
    aubCSR = AUBCommandStreamReceiver::create(hwInfoIn, "aubfile", false, executionEnvironment);
}

template <typename BaseCSR>
CommandStreamReceiverWithAUBDump<BaseCSR>::~CommandStreamReceiverWithAUBDump() {
    delete aubCSR;
}

template <typename BaseCSR>
FlushStamp CommandStreamReceiverWithAUBDump<BaseCSR>::flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    if (aubCSR) {
        aubCSR->flush(batchBuffer, engineOrdinal, allocationsForResidency, osContext);
    }
    FlushStamp flushStamp = BaseCSR::flush(batchBuffer, engineOrdinal, allocationsForResidency, osContext);
    return flushStamp;
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::makeNonResident(GraphicsAllocation &gfxAllocation) {
    int residencyTaskCount = gfxAllocation.residencyTaskCount[this->deviceIndex];
    BaseCSR::makeNonResident(gfxAllocation);
    gfxAllocation.residencyTaskCount[this->deviceIndex] = residencyTaskCount;
    if (aubCSR) {
        aubCSR->makeNonResident(gfxAllocation);
    }
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) {
    BaseCSR::activateAubSubCapture(dispatchInfo);
    if (aubCSR) {
        aubCSR->activateAubSubCapture(dispatchInfo);
    }
}

} // namespace OCLRT
