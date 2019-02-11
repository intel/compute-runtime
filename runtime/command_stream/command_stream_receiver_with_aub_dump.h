/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include <memory>

namespace OCLRT {

template <typename BaseCSR>
class CommandStreamReceiverWithAUBDump : public BaseCSR {
  protected:
    using BaseCSR::osContext;

  public:
    using BaseCSR::createMemoryManager;
    CommandStreamReceiverWithAUBDump(const HardwareInfo &hwInfoIn, const std::string &baseName, ExecutionEnvironment &executionEnvironment);

    CommandStreamReceiverWithAUBDump(const CommandStreamReceiverWithAUBDump &) = delete;
    CommandStreamReceiverWithAUBDump &operator=(const CommandStreamReceiverWithAUBDump &) = delete;

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;
    void setupContext(OsContext &osContext) override;

    std::unique_ptr<CommandStreamReceiver> aubCSR;
};

} // namespace OCLRT
