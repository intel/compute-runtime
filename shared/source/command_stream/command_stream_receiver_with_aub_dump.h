/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"

#include <memory>

namespace NEO {

template <typename BaseCSR>
class CommandStreamReceiverWithAUBDump : public BaseCSR {
  protected:
    using BaseCSR::osContext;

  public:
    CommandStreamReceiverWithAUBDump(const std::string &baseName,
                                     ExecutionEnvironment &executionEnvironment,
                                     uint32_t rootDeviceIndex,
                                     const DeviceBitfield deviceBitfield);

    CommandStreamReceiverWithAUBDump(const CommandStreamReceiverWithAUBDump &) = delete;
    CommandStreamReceiverWithAUBDump &operator=(const CommandStreamReceiverWithAUBDump &) = delete;

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override;
    void setupContext(OsContext &osContext) override;

    CommandStreamReceiverType getType() override {
        if (BaseCSR::getType() == CommandStreamReceiverType::CSR_TBX) {
            return CommandStreamReceiverType::CSR_TBX_WITH_AUB;
        }
        return CommandStreamReceiverType::CSR_HW_WITH_AUB;
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                                     bool useQuickKmdSleep, QueueThrottle throttle) override;

    size_t getPreferredTagPoolSize() const override { return 1; }

    void addAubComment(const char *comment) override;

    void pollForCompletion() override;

    bool expectMemory(const void *gfxAddress, const void *srcAddress,
                      size_t length, uint32_t compareOperation) override;

    std::unique_ptr<CommandStreamReceiver> aubCSR;
};

} // namespace NEO
