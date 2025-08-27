/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/submission_status.h"

struct COMMAND_BUFFER_HEADER_REC; // NOLINT(readability-identifier-naming), forward declaration from sharedata_wrapper.h

namespace NEO {
class GmmPageTableMngr;
class GraphicsAllocation;
class WddmMemoryManager;
class Wddm;

template <typename GfxFamily>
class WddmCommandStreamReceiver : public DeviceCommandStreamReceiver<GfxFamily> {
    typedef DeviceCommandStreamReceiver<GfxFamily> BaseClass;

  public:
    WddmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield);
    ~WddmCommandStreamReceiver() override;

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    SubmissionStatus processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) override;
    void processEviction() override;
    bool waitForFlushStamp(FlushStamp &flushStampToWait) override;
    bool isTlbFlushRequiredForStateCacheFlush() override;

    WddmMemoryManager *getMemoryManager() const;
    Wddm *peekWddm() const {
        return wddm;
    }
    GmmPageTableMngr *createPageTableManager() override;
    void flushMonitorFence(bool notifyKmd) override;
    void setupContext(OsContext &osContext) override;

    using CommandStreamReceiver::pageTableManager;

  protected:
    void addToEvictionContainer(GraphicsAllocation &gfxAllocation) override;
    bool validForEnqueuePagingFence(uint64_t pagingFenceValue) const;

    Wddm *wddm;
    COMMAND_BUFFER_HEADER_REC *commandBufferHeader;

    bool requiresBlockingResidencyHandling = true;
    uint64_t lastEnqueuedPagingFenceValue = 0;
};
} // namespace NEO
