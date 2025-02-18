/*
 * Copyright (C) 2018-2025 Intel Corporation
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
    using BaseCSR::pollForCompletion;
    using BaseCSR::writeMemory;

    CommandStreamReceiverWithAUBDump(const std::string &baseName,
                                     ExecutionEnvironment &executionEnvironment,
                                     uint32_t rootDeviceIndex,
                                     const DeviceBitfield deviceBitfield);

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override;
    void setupContext(OsContext &osContext) override;

    CommandStreamReceiverType getType() const override {
        if (BaseCSR::getType() == CommandStreamReceiverType::tbx) {
            return CommandStreamReceiverType::tbxWithAub;
        }
        return CommandStreamReceiverType::hardwareWithAub;
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait,
                                                     bool useQuickKmdSleep, QueueThrottle throttle) override;

    size_t getPreferredTagPoolSize() const override { return 1; }

    void addAubComment(const char *comment) override;

    void pollForCompletion(bool skipTaskCountCheck) override;
    void pollForAubCompletion() override;

    bool expectMemory(const void *gfxAddress, const void *srcAddress,
                      size_t length, uint32_t compareOperation) override;

    bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override;

    std::unique_ptr<CommandStreamReceiver> aubCSR;
};

} // namespace NEO
