/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"

#include <memory>

namespace NEO {

template <typename BaseCSR>
class CommandStreamReceiverWithAUBDump : public BaseCSR {
  protected:
    using BaseCSR::osContext;

  public:
    CommandStreamReceiverWithAUBDump(const std::string &baseName, ExecutionEnvironment &executionEnvironment);

    CommandStreamReceiverWithAUBDump(const CommandStreamReceiverWithAUBDump &) = delete;
    CommandStreamReceiverWithAUBDump &operator=(const CommandStreamReceiverWithAUBDump &) = delete;

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;
    void setupContext(OsContext &osContext) override;

    std::unique_ptr<CommandStreamReceiver> aubCSR;
};

} // namespace NEO
