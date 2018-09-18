/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"

namespace OCLRT {

template <typename BaseCSR>
class CommandStreamReceiverWithAUBDump : public BaseCSR {
  public:
    CommandStreamReceiverWithAUBDump(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment);
    ~CommandStreamReceiverWithAUBDump() override;

    CommandStreamReceiverWithAUBDump(const CommandStreamReceiverWithAUBDump &) = delete;
    CommandStreamReceiverWithAUBDump &operator=(const CommandStreamReceiverWithAUBDump &) = delete;

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer *allocationsForResidency, OsContext &osContext) override;
    void processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) override;

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;

    MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory) override;

    CommandStreamReceiver *aubCSR = nullptr;
};

} // namespace OCLRT
