/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/os_interface/linux/drm_gem_close_worker.h"
#include "drm/i915_drm.h"

#include <vector>

namespace OCLRT {
class BufferObject;
class Drm;
class DrmMemoryManager;

template <typename GfxFamily>
class DrmCommandStreamReceiver : public DeviceCommandStreamReceiver<GfxFamily> {
  protected:
    typedef DeviceCommandStreamReceiver<GfxFamily> BaseClass;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::getTagAddress;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::memoryManager;
    using BaseClass::getScratchPatchAddress;
    using BaseClass::hwInfo;
    using BaseClass::makeNonResident;
    using BaseClass::makeResident;
    using BaseClass::mediaVfeStateDirty;
    using BaseClass::requiredScratchSize;

  public:
    // When drm is null default implementation is used. In this case DrmCommandStreamReceiver is responsible to free drm.
    // When drm is passed, DCSR will not free it at destruction
    DrmCommandStreamReceiver(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment,
                             gemCloseWorkerMode mode = gemCloseWorkerMode::gemCloseWorkerActive);

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency, OsContext &osContext) override;
    void makeResident(GraphicsAllocation &gfxAllocation) override;
    void processResidency(ResidencyContainer *allocationsForResidency, OsContext &osContext) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;
    bool waitForFlushStamp(FlushStamp &flushStampToWait, OsContext &osContext) override;
    void overrideMediaVFEStateDirty(bool dirty) override;

    DrmMemoryManager *getMemoryManager();
    MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory) override;

    gemCloseWorkerMode peekGemCloseWorkerOperationMode() {
        return this->gemCloseWorkerOperationMode;
    }

  protected:
    void makeResident(BufferObject *bo);
    void programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags) override;

    std::vector<BufferObject *> residency;
    std::vector<drm_i915_gem_exec_object2> execObjectsStorage;
    Drm *drm;
    gemCloseWorkerMode gemCloseWorkerOperationMode;
    bool mediaVfeStateLowPriorityDirty = true;
};
} // namespace OCLRT
