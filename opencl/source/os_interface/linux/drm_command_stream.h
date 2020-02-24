/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_gem_close_worker.h"

#include "opencl/source/command_stream/device_command_stream.h"

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
    typedef DeviceCommandStreamReceiver<GfxFamily> BaseClass;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::getTagAddress;
    using BaseClass::getScratchPatchAddress;
    using BaseClass::makeNonResident;
    using BaseClass::makeResident;
    using BaseClass::mediaVfeStateDirty;
    using BaseClass::osContext;
    using BaseClass::requiredScratchSize;

  public:
    // When drm is null default implementation is used. In this case DrmCommandStreamReceiver is responsible to free drm.
    // When drm is passed, DCSR will not free it at destruction
    DrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex,
                             gemCloseWorkerMode mode = gemCloseWorkerMode::gemCloseWorkerActive);

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;
    bool waitForFlushStamp(FlushStamp &flushStampToWait) override;

    DrmMemoryManager *getMemoryManager() const;
    GmmPageTableMngr *createPageTableManager() override;

    gemCloseWorkerMode peekGemCloseWorkerOperationMode() const {
        return this->gemCloseWorkerOperationMode;
    }

  protected:
    void makeResidentBufferObjects(const DrmAllocation *drmAllocation, uint32_t handleId);
    void makeResident(BufferObject *bo);
    void flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency);
    void exec(const BatchBuffer &batchBuffer, uint32_t drmContextId);

    std::vector<BufferObject *> residency;
    std::vector<drm_i915_gem_exec_object2> execObjectsStorage;
    Drm *drm;
    gemCloseWorkerMode gemCloseWorkerOperationMode;
};
} // namespace NEO
