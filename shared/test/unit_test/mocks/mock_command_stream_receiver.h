/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"

#include "gmock/gmock.h"

#include <vector>

using namespace NEO;

class MockCommandStreamReceiver : public CommandStreamReceiver {
  public:
    using CommandStreamReceiver::CommandStreamReceiver;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::internalAllocationStorage;
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiver::requiredThreadArbitrationPolicy;
    using CommandStreamReceiver::tagAddress;

    std::vector<char> instructionHeapReserveredData;
    int *flushBatchedSubmissionsCallCounter = nullptr;
    uint32_t waitForCompletionWithTimeoutCalled = 0;
    bool multiOsContextCapable = false;
    bool downloadAllocationsCalled = false;

    bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait) override {
        waitForCompletionWithTimeoutCalled++;
        return true;
    }
    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    bool isMultiOsContextCapable() const override { return multiOsContextCapable; }

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap &dsh,
        const IndirectHeap &ioh,
        const IndirectHeap &ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override;

    bool flushBatchedSubmissions() override {
        if (flushBatchedSubmissionsCallCounter) {
            (*flushBatchedSubmissionsCallCounter)++;
        }
        return true;
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, bool forcePowerSavingMode) override {
    }

    uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking) override { return taskCount; };

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_HW;
    }

    void downloadAllocations() override {
        downloadAllocationsCalled = true;
    }
};
