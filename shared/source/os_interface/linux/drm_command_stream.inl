/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"

#include <cstdlib>
#include <cstring>

namespace NEO {

template <typename GfxFamily>
DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                                              uint32_t rootDeviceIndex,
                                                              const DeviceBitfield deviceBitfield,
                                                              gemCloseWorkerMode mode)
    : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield), gemCloseWorkerOperationMode(mode) {

    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get();

    this->drm = rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>();
    residency.reserve(512);
    execObjectsStorage.reserve(512);

    if (this->drm->isVmBindAvailable()) {
        gemCloseWorkerOperationMode = gemCloseWorkerMode::gemCloseWorkerInactive;
    }

    if (DebugManager.flags.EnableGemCloseWorker.get() != -1) {
        gemCloseWorkerOperationMode = DebugManager.flags.EnableGemCloseWorker.get() ? gemCloseWorkerMode::gemCloseWorkerActive : gemCloseWorkerMode::gemCloseWorkerInactive;
    }

    auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
    auto localMemoryEnabled = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*hwInfo);

    this->dispatchMode = localMemoryEnabled ? DispatchMode::BatchedDispatch : DispatchMode::ImmediateDispatch;

    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = static_cast<DispatchMode>(DebugManager.flags.CsrDispatchMode.get());
    }
    int overrideUserFenceForCompletionWait = DebugManager.flags.EnableUserFenceForCompletionWait.get();
    if (overrideUserFenceForCompletionWait != -1) {
        useUserFenceWait = !!(overrideUserFenceForCompletionWait);
    }
    int overrideUserFenceUseCtxId = DebugManager.flags.EnableUserFenceUseCtxId.get();
    if (overrideUserFenceUseCtxId != -1) {
        useContextForUserFenceWait = !!(overrideUserFenceUseCtxId);
    }
    useNotifyEnableForPostSync = useUserFenceWait;
    int overrideUseNotifyEnableForPostSync = DebugManager.flags.OverrideNotifyEnableForTagUpdatePostSync.get();
    if (overrideUseNotifyEnableForPostSync != -1) {
        useNotifyEnableForPostSync = !!(overrideUseNotifyEnableForPostSync);
    }
    kmdWaitTimeout = DebugManager.flags.SetKmdWaitTimeout.get();
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    this->printDeviceIndex();
    DrmAllocation *alloc = static_cast<DrmAllocation *>(batchBuffer.commandBufferAllocation);
    DEBUG_BREAK_IF(!alloc);

    BufferObject *bb = alloc->getBO();
    if (bb == nullptr) {
        return false;
    }

    if (this->lastSentSliceCount != batchBuffer.sliceCount) {
        if (drm->setQueueSliceCount(batchBuffer.sliceCount)) {
            this->lastSentSliceCount = batchBuffer.sliceCount;
        }
    }

    auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface.get());

    std::unique_lock<std::mutex> lock;
    if (!this->directSubmission.get() && !this->blitterDirectSubmission.get()) {
        lock = memoryOperationsInterface->lockHandlerIfUsed();
    }

    this->printBOsForSubmit(allocationsForResidency, *batchBuffer.commandBufferAllocation);

    memoryOperationsInterface->mergeWithResidencyContainer(this->osContext, allocationsForResidency);

    if (this->drm->isVmBindAvailable()) {
        memoryOperationsInterface->makeResidentWithinOsContext(this->osContext, ArrayRef<GraphicsAllocation *>(&batchBuffer.commandBufferAllocation, 1), true);
    }

    if (this->directSubmission.get()) {
        return this->directSubmission->dispatchCommandBuffer(batchBuffer, *this->flushStamp.get());
    }
    if (this->blitterDirectSubmission.get()) {
        return this->blitterDirectSubmission->dispatchCommandBuffer(batchBuffer, *this->flushStamp.get());
    }

    if (isUserFenceWaitActive()) {
        this->flushStamp->setStamp(latestSentTaskCount);
    } else {
        this->flushStamp->setStamp(bb->peekHandle());
    }
    auto ret = this->flushInternal(batchBuffer, allocationsForResidency);

    if (this->gemCloseWorkerOperationMode == gemCloseWorkerMode::gemCloseWorkerActive) {
        bb->reference();
        this->getMemoryManager()->peekGemCloseWorker()->push(bb);
    }

    if (ret) {
        return false;
    }

    return true;
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::printBOsForSubmit(ResidencyContainer &allocationsForResidency, GraphicsAllocation &cmdBufferAllocation) {
    if (DebugManager.flags.PrintBOsForSubmit.get()) {
        std::vector<BufferObject *> bosForSubmit;
        for (auto drmIterator = 0u; drmIterator < osContext->getDeviceBitfield().size(); drmIterator++) {
            if (osContext->getDeviceBitfield().test(drmIterator)) {
                for (auto gfxAllocation = allocationsForResidency.begin(); gfxAllocation != allocationsForResidency.end(); gfxAllocation++) {
                    auto drmAllocation = static_cast<DrmAllocation *>(*gfxAllocation);
                    drmAllocation->makeBOsResident(osContext, drmIterator, &bosForSubmit, true);
                }
                auto drmCmdBufferAllocation = static_cast<DrmAllocation *>(&cmdBufferAllocation);
                drmCmdBufferAllocation->makeBOsResident(osContext, drmIterator, &bosForSubmit, true);
            }
        }
        printf("Buffer object for submit\n");
        for (const auto &bo : bosForSubmit) {
            printf("BO-%d, range: %" SCNx64 " - %" SCNx64 ", size: %" SCNdPTR "\n", bo->peekHandle(), bo->peekAddress(), ptrOffset(bo->peekAddress(), bo->peekSize()), bo->peekSize());
        }
        printf("\n");
    }
}

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId) {
    DrmAllocation *alloc = static_cast<DrmAllocation *>(batchBuffer.commandBufferAllocation);
    DEBUG_BREAK_IF(!alloc);
    BufferObject *bb = alloc->getBO();
    DEBUG_BREAK_IF(!bb);

    auto execFlags = static_cast<OsContextLinux *>(osContext)->getEngineFlag() | I915_EXEC_NO_RELOC;

    // Residency hold all allocation except command buffer, hence + 1
    auto requiredSize = this->residency.size() + 1;
    if (requiredSize > this->execObjectsStorage.size()) {
        this->execObjectsStorage.resize(requiredSize);
    }

    int ret = bb->exec(static_cast<uint32_t>(alignUp(batchBuffer.usedSize - batchBuffer.startOffset, 8)),
                       batchBuffer.startOffset, execFlags,
                       batchBuffer.requiresCoherency,
                       this->osContext,
                       vmHandleId,
                       drmContextId,
                       this->residency.data(), this->residency.size(),
                       this->execObjectsStorage.data());

    this->residency.clear();

    return ret;
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::processResidency(const ResidencyContainer &inputAllocationsForResidency, uint32_t handleId) {
    for (auto &alloc : inputAllocationsForResidency) {
        auto drmAlloc = static_cast<DrmAllocation *>(alloc);
        drmAlloc->makeBOsResident(osContext, handleId, &this->residency, false);
    }
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::makeNonResident(GraphicsAllocation &gfxAllocation) {
    // Vector is moved to command buffer inside flush.
    // If flush wasn't called we need to make all objects non-resident.
    // If makeNonResident is called before flush, vector will be cleared.
    if (gfxAllocation.isResident(this->osContext->getContextId())) {
        if (this->residency.size() != 0) {
            this->residency.clear();
        }
        for (auto fragmentId = 0u; fragmentId < gfxAllocation.fragmentsStorage.fragmentCount; fragmentId++) {
            gfxAllocation.fragmentsStorage.fragmentStorageData[fragmentId].residency->resident[osContext->getContextId()] = false;
        }
    }
    gfxAllocation.releaseResidencyInOsContext(this->osContext->getContextId());
}

template <typename GfxFamily>
DrmMemoryManager *DrmCommandStreamReceiver<GfxFamily>::getMemoryManager() const {
    return static_cast<DrmMemoryManager *>(CommandStreamReceiver::getMemoryManager());
}

template <typename GfxFamily>
GmmPageTableMngr *DrmCommandStreamReceiver<GfxFamily>::createPageTableManager() {
    auto rootDeviceEnvironment = this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex].get();
    auto gmmClientContext = rootDeviceEnvironment->getGmmClientContext();

    GMM_DEVICE_INFO deviceInfo{};
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
    deviceInfo.pDeviceCb = &deviceCallbacks;
    gmmClientContext->setGmmDeviceInfo(&deviceInfo);

    auto gmmPageTableMngr = GmmPageTableMngr::create(gmmClientContext, TT_TYPE::AUXTT, nullptr);
    gmmPageTableMngr->setCsrHandle(this);

    this->pageTableManager.reset(gmmPageTableMngr);

    return gmmPageTableMngr;
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStamp) {
    auto waitValue = static_cast<uint32_t>(flushStamp);
    if (isUserFenceWaitActive()) {
        waitUserFence(waitValue);
    } else {
        this->drm->waitHandle(waitValue, kmdWaitTimeout);
    }

    return true;
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::isKmdWaitModeActive() {
    if (this->drm->isVmBindAvailable()) {
        return useUserFenceWait;
    }
    return true;
}

template <typename GfxFamily>
inline bool DrmCommandStreamReceiver<GfxFamily>::isUserFenceWaitActive() {
    return (this->drm->isVmBindAvailable() && useUserFenceWait);
}

} // namespace NEO
