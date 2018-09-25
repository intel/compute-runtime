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
    FlushStamp flushStamp = BaseCSR::flush(batchBuffer, engineOrdinal, allocationsForResidency, osContext);
    if (aubCSR) {
        aubCSR->flush(batchBuffer, engineOrdinal, allocationsForResidency, osContext);
    }
    return flushStamp;
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    BaseCSR::processResidency(allocationsForResidency, osContext);
    if (aubCSR) {
        aubCSR->processResidency(allocationsForResidency, osContext);
    }
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) {
    BaseCSR::activateAubSubCapture(dispatchInfo);
    if (aubCSR) {
        aubCSR->activateAubSubCapture(dispatchInfo);
    }
}

template <typename BaseCSR>
MemoryManager *CommandStreamReceiverWithAUBDump<BaseCSR>::createMemoryManager(bool enable64kbPages, bool enableLocalMemory) {
    auto memoryManager = BaseCSR::createMemoryManager(enable64kbPages, enableLocalMemory);
    if (aubCSR) {
        aubCSR->setMemoryManager(memoryManager);
    }
    return memoryManager;
}

} // namespace OCLRT
