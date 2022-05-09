/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/submission_status.h"

struct COMMAND_BUFFER_HEADER_REC;

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
    void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) override;
    void processEviction() override;
    bool waitForFlushStamp(FlushStamp &flushStampToWait) override;

    WddmMemoryManager *getMemoryManager() const;
    Wddm *peekWddm() const {
        return wddm;
    }
    GmmPageTableMngr *createPageTableManager() override;

    using CommandStreamReceiver::pageTableManager;

  protected:
    void kmDafLockAllocations(ResidencyContainer &allocationsForResidency);

    Wddm *wddm;
    COMMAND_BUFFER_HEADER_REC *commandBufferHeader;
};
} // namespace NEO
