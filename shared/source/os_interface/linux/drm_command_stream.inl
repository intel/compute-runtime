/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/sys_calls_common.h"

namespace NEO {

template <typename GfxFamily>
DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                                              uint32_t rootDeviceIndex,
                                                              const DeviceBitfield deviceBitfield)
    : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield), vmBindAvailable(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>()->isVmBindAvailable()) {

    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get();

    this->drm = rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>();
    residency.reserve(512);
    execObjectsStorage.reserve(512);

    auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
    auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
    auto localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(*hwInfo);

    this->dispatchMode = localMemoryEnabled ? DispatchMode::batchedDispatch : DispatchMode::immediateDispatch;

    if (ApiSpecificConfig::getApiType() == ApiSpecificConfig::L0) {
        this->dispatchMode = DispatchMode::immediateDispatch;
    }

    if (debugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = static_cast<DispatchMode>(debugManager.flags.CsrDispatchMode.get());
    }
    int overrideUserFenceForCompletionWait = debugManager.flags.EnableUserFenceForCompletionWait.get();
    if (overrideUserFenceForCompletionWait != -1) {
        useUserFenceWait = !!(overrideUserFenceForCompletionWait);
    }
    useNotifyEnableForPostSync = useUserFenceWait;
    int overrideUseNotifyEnableForPostSync = debugManager.flags.OverrideNotifyEnableForTagUpdatePostSync.get();
    if (overrideUseNotifyEnableForPostSync != -1) {
        useNotifyEnableForPostSync = !!(overrideUseNotifyEnableForPostSync);
    }
    kmdWaitTimeout = debugManager.flags.SetKmdWaitTimeout.get();
}

template <typename GfxFamily>
inline DrmCommandStreamReceiver<GfxFamily>::~DrmCommandStreamReceiver() {
    if (this->isUpdateTagFromWaitEnabled()) {
        this->waitForCompletionWithTimeout(WaitParams{false, false, false, 0}, this->peekTaskCount());
    }

    this->getMemoryManager()->drainGemCloseWorker();
}

template <typename GfxFamily>
SubmissionStatus DrmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (debugManager.flags.ExitOnSubmissionNumber.get() != -1) {
        bool enabled = (this->taskCount >= static_cast<TaskCountType>(debugManager.flags.ExitOnSubmissionNumber.get()));

        if (debugManager.flags.ExitOnSubmissionMode.get() == 1 && !EngineHelpers::isComputeEngine(this->osContext->getEngineType())) {
            enabled = false;
        }

        if (debugManager.flags.ExitOnSubmissionMode.get() == 2 && !EngineHelpers::isBcs(this->osContext->getEngineType())) {
            enabled = false;
        }

        if (enabled) {
            SysCalls::exit(0);
        }
    }

    this->printDeviceIndex();
    DrmAllocation *alloc = static_cast<DrmAllocation *>(batchBuffer.commandBufferAllocation);
    DEBUG_BREAK_IF(!alloc);

    BufferObject *bb = alloc->getBO();
    if (bb == nullptr) {
        return SubmissionStatus::outOfMemory;
    }

    if (this->lastSentSliceCount != batchBuffer.sliceCount) {
        if (drm->setQueueSliceCount(batchBuffer.sliceCount)) {
            this->lastSentSliceCount = batchBuffer.sliceCount;
        }
    }

    auto memoryOperationsInterface = static_cast<DrmMemoryOperationsHandler *>(this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface.get());

    std::unique_lock<std::mutex> lock;
    if (!this->vmBindAvailable) {
        lock = memoryOperationsInterface->lockHandlerIfUsed();
    }

    auto submissionStatus = this->printBOsForSubmit(allocationsForResidency, *batchBuffer.commandBufferAllocation);
    if (submissionStatus != SubmissionStatus::success) {
        return submissionStatus;
    }

    if (this->vmBindAvailable) {
        allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);
    }

    MemoryOperationsStatus retVal = memoryOperationsInterface->mergeWithResidencyContainer(this->osContext, allocationsForResidency);
    if (retVal != MemoryOperationsStatus::success) {
        if (retVal == MemoryOperationsStatus::outOfMemory) {
            return SubmissionStatus::outOfMemory;
        }
        return SubmissionStatus::failed;
    }

    if (this->directSubmission.get()) {
        if (!this->vmBindAvailable) {
            batchBuffer.allocationsForResidency = &allocationsForResidency;
        }
        bool ret = this->directSubmission->dispatchCommandBuffer(batchBuffer, *this->flushStamp.get());
        if (ret == false) {
            return Drm::getSubmissionStatusFromReturnCode(this->directSubmission->getDispatchErrorCode());
        }
        return SubmissionStatus::success;
    }
    if (this->blitterDirectSubmission.get()) {
        bool ret = this->blitterDirectSubmission->dispatchCommandBuffer(batchBuffer, *this->flushStamp.get());
        if (ret == false) {
            return Drm::getSubmissionStatusFromReturnCode(this->blitterDirectSubmission->getDispatchErrorCode());
        }
        return SubmissionStatus::success;
    }

    if (isUserFenceWaitActive()) {
        this->flushStamp->setStamp(latestSentTaskCount);
    } else {
        this->flushStamp->setStamp(bb->peekHandle());
    }

    auto readBackMode = debugManager.flags.ReadBackCommandBufferAllocation.get();
    bool readBackAllowed = ((batchBuffer.commandBufferAllocation->isAllocatedInLocalMemoryPool() && readBackMode == 1) || readBackMode == 2);

    if (readBackAllowed) {
        readBackAllocation(ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset));
    }

    auto ret = this->flushInternal(batchBuffer, allocationsForResidency);

    if (this->isGemCloseWorkerActive()) {
        bb->reference();
        this->getMemoryManager()->peekGemCloseWorker()->push(bb);
    }

    return ret;
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::readBackAllocation(void *source) {
    reserved = *reinterpret_cast<volatile uint32_t *>(source);
}

template <typename GfxFamily>
SubmissionStatus DrmCommandStreamReceiver<GfxFamily>::printBOsForSubmit(ResidencyContainer &allocationsForResidency, GraphicsAllocation &cmdBufferAllocation) {
    if (debugManager.flags.PrintBOsForSubmit.get()) {
        std::vector<BufferObject *> bosForSubmit;
        for (auto drmIterator = 0u; drmIterator < osContext->getDeviceBitfield().size(); drmIterator++) {
            if (osContext->getDeviceBitfield().test(drmIterator)) {
                for (auto gfxAllocation = allocationsForResidency.begin(); gfxAllocation != allocationsForResidency.end(); gfxAllocation++) {
                    auto drmAllocation = static_cast<DrmAllocation *>(*gfxAllocation);
                    auto retCode = drmAllocation->makeBOsResident(osContext, drmIterator, &bosForSubmit, true, false);
                    if (retCode) {
                        return Drm::getSubmissionStatusFromReturnCode(retCode);
                    }
                }
                auto drmCmdBufferAllocation = static_cast<DrmAllocation *>(&cmdBufferAllocation);
                auto retCode = drmCmdBufferAllocation->makeBOsResident(osContext, drmIterator, &bosForSubmit, true, false);
                if (retCode) {
                    return Drm::getSubmissionStatusFromReturnCode(retCode);
                }
            }
        }
        PRINT_DEBUG_STRING(true, stdout, "Buffer object for submit\n");
        for (const auto &bo : bosForSubmit) {
            PRINT_DEBUG_STRING(true, stdout, "BO-%d, range: %" SCNx64 " - %" SCNx64 ", size: %" SCNdPTR "\n", bo->peekHandle(), bo->peekAddress(), ptrOffset(bo->peekAddress(), bo->peekSize()), bo->peekSize());
        }
        PRINT_DEBUG_STRING(true, stdout, "\n");
    }
    return SubmissionStatus::success;
}

template <typename GfxFamily>
int DrmCommandStreamReceiver<GfxFamily>::exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) {
    DrmAllocation *alloc = static_cast<DrmAllocation *>(batchBuffer.commandBufferAllocation);
    DEBUG_BREAK_IF(!alloc);
    BufferObject *bb = alloc->getBO();
    DEBUG_BREAK_IF(!bb);

    auto osContextLinux = static_cast<OsContextLinux *>(this->osContext);
    auto execFlags = osContextLinux->getEngineFlag() | drm->getIoctlHelper()->getDrmParamValue(DrmParam::execNoReloc);

    // requiredSize determinant:
    // * vmBind UNAVAILABLE => residency holds all allocations except for the command buffer
    // * vmBind AVAILABLE   => residency holds command buffer as well
    auto requiredSize = this->residency.size() + 1;
    if (requiredSize > this->execObjectsStorage.size()) {
        this->execObjectsStorage.resize(requiredSize);
    }

    uint64_t completionGpuAddress = 0;
    TaskCountType completionValue = 0;
    if (this->vmBindAvailable && this->drm->completionFenceSupport()) {
        completionGpuAddress = getTagAllocation()->getGpuAddress() + (index * this->immWritePostSyncWriteOffset) + TagAllocationLayout::completionFenceOffset;
        completionValue = this->latestSentTaskCount;
    }

    int ret = bb->exec(static_cast<uint32_t>(alignUp(batchBuffer.usedSize - batchBuffer.startOffset, 8)),
                       batchBuffer.startOffset, execFlags,
                       false,
                       this->osContext,
                       vmHandleId,
                       drmContextId,
                       this->residency.data(), this->residency.size(),
                       this->execObjectsStorage.data(),
                       completionGpuAddress,
                       completionValue);

    this->residency.clear();

    return ret;
}

template <typename GfxFamily>
SubmissionStatus DrmCommandStreamReceiver<GfxFamily>::processResidency(ResidencyContainer &inputAllocationsForResidency, uint32_t handleId) {
    if (this->vmBindAvailable) {
        return SubmissionStatus::success;
    }
    int ret = 0;
    for (auto &alloc : inputAllocationsForResidency) {
        auto drmAlloc = static_cast<DrmAllocation *>(alloc);
        ret = drmAlloc->makeBOsResident(osContext, handleId, &this->residency, false, false);
        if (ret != 0) {
            break;
        }
    }

    return Drm::getSubmissionStatusFromReturnCode(ret);
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

    if (!gfxAllocation.isAlwaysResident(this->osContext->getContextId())) {
        gfxAllocation.releaseResidencyInOsContext(this->osContext->getContextId());
    }
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

    this->pageTableManager.reset(gmmPageTableMngr);

    return gmmPageTableMngr;
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStamp) {
    auto waitValue = static_cast<uint32_t>(flushStamp);
    if (isUserFenceWaitActive()) {
        uint64_t tagAddress = castToUint64(const_cast<TagAddressType *>(getTagAddress()));
        return waitUserFence(waitValue, tagAddress, kmdWaitTimeout, false, NEO::InterruptId::notUsed, nullptr);
    } else {
        this->drm->waitHandle(waitValue, kmdWaitTimeout);
    }

    return true;
}

template <typename GfxFamily>
SubmissionStatus DrmCommandStreamReceiver<GfxFamily>::flushInternal(const BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (drm->useVMBindImmediate()) {
        auto osContextLinux = static_cast<OsContextLinux *>(this->osContext);
        osContextLinux->waitForPagingFence();
    }

    auto &drmContextIds = static_cast<const OsContextLinux *>(osContext)->getDrmContextIds();

    uint32_t contextIndex = 0;
    for (auto tileIterator = 0u; tileIterator < this->osContext->getDeviceBitfield().size(); tileIterator++) {
        if (this->osContext->getDeviceBitfield().test(tileIterator)) {
            if (debugManager.flags.ForceExecutionTile.get() != -1 && this->osContext->getDeviceBitfield().count() > 1) {
                tileIterator = contextIndex = debugManager.flags.ForceExecutionTile.get();
            }

            auto processResidencySuccess = this->processResidency(allocationsForResidency, tileIterator);
            if (processResidencySuccess != SubmissionStatus::success) {
                return processResidencySuccess;
            }

            if (debugManager.flags.PrintDeviceAndEngineIdOnSubmission.get()) {
                printf("%u: Drm Submission of contextIndex: %u, with context id %u\n", SysCalls::getProcessId(), contextIndex, drmContextIds[contextIndex]);
            }

            int ret = this->exec(batchBuffer, tileIterator, drmContextIds[contextIndex], contextIndex);
            if (ret) {
                return Drm::getSubmissionStatusFromReturnCode(ret);
            }

            contextIndex++;

            if (debugManager.flags.EnableWalkerPartition.get() == 0) {
                return SubmissionStatus::success;
            }
        }
    }

    return SubmissionStatus::success;
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::waitUserFence(TaskCountType waitValue, uint64_t hostAddress, int64_t timeout, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) {
    int ret = drm->waitOnUserFences(static_cast<OsContextLinux &>(*this->osContext), hostAddress, waitValue, this->activePartitions, timeout, this->immWritePostSyncWriteOffset, userInterrupt, externalInterruptId, allocForInterruptWait);

    return (ret == 0);
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::isGemCloseWorkerActive() const {
    return this->getMemoryManager()->peekGemCloseWorker() && !this->osContext->isInternalEngine() && !this->osContext->isDirectSubmissionLightActive() && this->getType() == CommandStreamReceiverType::hardware;
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::isKmdWaitModeActive() {
    if (this->vmBindAvailable) {
        return useUserFenceWait;
    }
    return true;
}

template <typename GfxFamily>
inline bool DrmCommandStreamReceiver<GfxFamily>::isUserFenceWaitActive() {
    return (this->vmBindAvailable && useUserFenceWait);
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::isKmdWaitOnTaskCountAllowed() const {
    return this->isDirectSubmissionEnabled() && this->vmBindAvailable;
}
} // namespace NEO
