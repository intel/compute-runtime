/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"

#include "drm/i915_drm.h"

#include <vector>

namespace NEO {
class BufferObject;
class Drm;
class DrmAllocation;
class DrmMemoryManager;

template <typename GfxFamily>
class DrmCommandStreamReceiver : public DeviceCommandStreamReceiver<GfxFamily> {
  protected:
    using BaseClass = DeviceCommandStreamReceiver<GfxFamily>;

    using BaseClass::getScratchPatchAddress;
    using BaseClass::makeNonResident;
    using BaseClass::makeResident;
    using BaseClass::mediaVfeStateDirty;
    using BaseClass::osContext;
    using BaseClass::requiredScratchSize;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::getTagAddress;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::getTagAllocation;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::taskCount;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::useNotifyEnableForPostSync;

  public:
    // When drm is null default implementation is used. In this case DrmCommandStreamReceiver is responsible to free drm.
    // When drm is passed, DCSR will not free it at destruction
    DrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                             uint32_t rootDeviceIndex,
                             const DeviceBitfield deviceBitfield,
                             gemCloseWorkerMode mode = gemCloseWorkerMode::gemCloseWorkerActive);
    ~DrmCommandStreamReceiver();

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    MOCKABLE_VIRTUAL void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;
    bool waitForFlushStamp(FlushStamp &flushStampToWait) override;
    bool isKmdWaitModeActive() override;

    DrmMemoryManager *getMemoryManager() const;
    GmmPageTableMngr *createPageTableManager() override;

    gemCloseWorkerMode peekGemCloseWorkerOperationMode() const {
        return this->gemCloseWorkerOperationMode;
    }

    void initializeDefaultsForInternalEngine() override {
        gemCloseWorkerOperationMode = gemCloseWorkerMode::gemCloseWorkerInactive;
    }

    uint64_t getCompletionAddress() override;

    uint32_t getCompletionValue(const GraphicsAllocation &gfxAllocation) override;

    void printBOsForSubmit(ResidencyContainer &allocationsForResidency, GraphicsAllocation &cmdBufferAllocation);

    using CommandStreamReceiver::pageTableManager;

  protected:
    MOCKABLE_VIRTUAL int flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency);
    MOCKABLE_VIRTUAL int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index);
    MOCKABLE_VIRTUAL int waitUserFence(uint32_t waitValue);
    MOCKABLE_VIRTUAL void readBackAllocation(void *source);
    bool isUserFenceWaitActive();

    std::vector<BufferObject *> residency;
    std::vector<drm_i915_gem_exec_object2> execObjectsStorage;
    Drm *drm;
    gemCloseWorkerMode gemCloseWorkerOperationMode;

    volatile uint32_t reserved = 0;
    int32_t kmdWaitTimeout = -1;

    bool useUserFenceWait = true;
    bool useContextForUserFenceWait = false;
};
} // namespace NEO
