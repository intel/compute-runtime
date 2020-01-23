/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/linear_stream.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/gmm_helper/page_table_mngr.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/flush_stamp.h"
#include "core/helpers/preamble.h"
#include "core/memory_manager/residency.h"
#include "core/os_interface/linux/drm_engine_mapper.h"
#include "core/os_interface/linux/drm_neo.h"
#include "core/os_interface/linux/os_interface.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/platform/platform.h"

#include <cstdlib>
#include <cstring>

namespace NEO {

template <typename GfxFamily>
DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, gemCloseWorkerMode mode)
    : BaseClass(executionEnvironment, rootDeviceIndex), gemCloseWorkerOperationMode(mode) {

    this->drm = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->getDrm();

    residency.reserve(512);
    execObjectsStorage.reserve(512);

    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->setDrm(this->drm);
    CommandStreamReceiver::osInterface = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface.get();
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
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

    this->flushStamp->setStamp(bb->peekHandle());
    this->flushInternal(batchBuffer, allocationsForResidency);

    if (this->gemCloseWorkerOperationMode == gemCloseWorkerMode::gemCloseWorkerActive) {
        bb->reference();
        this->getMemoryManager()->peekGemCloseWorker()->push(bb);
    }

    return true;
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::exec(const BatchBuffer &batchBuffer, uint32_t drmContextId) {
    DrmAllocation *alloc = static_cast<DrmAllocation *>(batchBuffer.commandBufferAllocation);
    DEBUG_BREAK_IF(!alloc);
    BufferObject *bb = alloc->getBO();
    DEBUG_BREAK_IF(!bb);

    auto engineFlag = static_cast<OsContextLinux *>(osContext)->getEngineFlag();

    // Residency hold all allocation except command buffer, hence + 1
    auto requiredSize = this->residency.size() + 1;
    if (requiredSize > this->execObjectsStorage.size()) {
        this->execObjectsStorage.resize(requiredSize);
    }

    int err = bb->exec(static_cast<uint32_t>(alignUp(batchBuffer.usedSize - batchBuffer.startOffset, 8)),
                       batchBuffer.startOffset, engineFlag | I915_EXEC_NO_RELOC,
                       batchBuffer.requiresCoherency,
                       drmContextId,
                       this->residency.data(), this->residency.size(),
                       this->execObjectsStorage.data());
    UNRECOVERABLE_IF(err != 0);

    this->residency.clear();
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::makeResident(BufferObject *bo) {
    if (bo) {
        if (bo->peekIsReusableAllocation()) {
            for (auto bufferObject : this->residency) {
                if (bufferObject == bo) {
                    return;
                }
            }
        }

        residency.push_back(bo);
    }
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::processResidency(const ResidencyContainer &inputAllocationsForResidency) {
    for (auto &alloc : inputAllocationsForResidency) {
        auto drmAlloc = static_cast<const DrmAllocation *>(alloc);
        if (drmAlloc->fragmentsStorage.fragmentCount) {
            for (unsigned int f = 0; f < drmAlloc->fragmentsStorage.fragmentCount; f++) {
                const auto osContextId = osContext->getContextId();
                if (!drmAlloc->fragmentsStorage.fragmentStorageData[f].residency->resident[osContextId]) {
                    makeResident(drmAlloc->fragmentsStorage.fragmentStorageData[f].osHandleStorage->bo);
                    drmAlloc->fragmentsStorage.fragmentStorageData[f].residency->resident[osContextId] = true;
                }
            }
        } else {
            makeResidentBufferObjects(drmAlloc);
        }
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
    GmmPageTableMngr *gmmPageTableMngr = GmmPageTableMngr::create(this->executionEnvironment.getGmmClientContext(), TT_TYPE::AUXTT, nullptr);
    gmmPageTableMngr->setCsrHandle(this);
    this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->pageTableManager.reset(gmmPageTableMngr);
    return gmmPageTableMngr;
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStamp) {
    drm_i915_gem_wait wait = {};
    wait.bo_handle = static_cast<uint32_t>(flushStamp);
    wait.timeout_ns = -1;

    drm->ioctl(DRM_IOCTL_I915_GEM_WAIT, &wait);
    return true;
}

} // namespace NEO
