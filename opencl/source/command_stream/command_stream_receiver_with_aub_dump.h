/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"

#include <memory>

namespace NEO {

template <typename BaseCSR>
class CommandStreamReceiverWithAUBDump : public BaseCSR {
  protected:
    using BaseCSR::osContext;

  public:
    CommandStreamReceiverWithAUBDump(const std::string &baseName, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

    CommandStreamReceiverWithAUBDump(const CommandStreamReceiverWithAUBDump &) = delete;
    CommandStreamReceiverWithAUBDump &operator=(const CommandStreamReceiverWithAUBDump &) = delete;

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;
    void setupContext(OsContext &osContext) override;

    CommandStreamReceiverType getType() override {
        if (BaseCSR::getType() == CommandStreamReceiverType::CSR_TBX) {
            return CommandStreamReceiverType::CSR_TBX_WITH_AUB;
        }
        return CommandStreamReceiverType::CSR_HW_WITH_AUB;
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                               bool useQuickKmdSleep, bool forcePowerSavingMode) override;

    size_t getPreferredTagPoolSize() const override { return 1; }

    void addAubComment(const char *comment) override;

    std::unique_ptr<CommandStreamReceiver> aubCSR;
};

} // namespace NEO
