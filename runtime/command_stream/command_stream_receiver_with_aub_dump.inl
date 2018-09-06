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
FlushStamp CommandStreamReceiverWithAUBDump<BaseCSR>::flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer *allocationsForResidency, OsContext &osContext) {
    FlushStamp flushStamp = BaseCSR::flush(batchBuffer, engineOrdinal, allocationsForResidency, osContext);
    if (aubCSR) {
        aubCSR->flush(batchBuffer, engineOrdinal, allocationsForResidency, osContext);
    }
    return flushStamp;
}

template <typename BaseCSR>
void CommandStreamReceiverWithAUBDump<BaseCSR>::processResidency(ResidencyContainer *allocationsForResidency, OsContext &osContext) {
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
