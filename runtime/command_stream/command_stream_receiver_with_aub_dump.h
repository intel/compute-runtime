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
    using BaseCSR::createMemoryManager;
    CommandStreamReceiverWithAUBDump(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment);
    ~CommandStreamReceiverWithAUBDump() override;

    CommandStreamReceiverWithAUBDump(const CommandStreamReceiverWithAUBDump &) = delete;
    CommandStreamReceiverWithAUBDump &operator=(const CommandStreamReceiverWithAUBDump &) = delete;

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer &allocationsForResidency, OsContext &osContext) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;

    CommandStreamReceiver *aubCSR = nullptr;
};

} // namespace OCLRT
