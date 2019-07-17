/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/linear_stream.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/preamble.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/platform/platform.h"

#include "hw_cmds.h"

#include <cstdlib>
#include <cstring>

namespace NEO {

template <typename GfxFamily>
DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, gemCloseWorkerMode mode)
    : BaseClass(executionEnvironment), gemCloseWorkerOperationMode(mode) {

    this->drm = executionEnvironment.osInterface->get()->getDrm();

    residency.reserve(512);
    execObjectsStorage.reserve(512);

    executionEnvironment.osInterface->get()->setDrm(this->drm);
    CommandStreamReceiver::osInterface = executionEnvironment.osInterface.get();
    auto gmmHelper = platform()->peekExecutionEnvironment()->getGmmHelper();
    gmmHelper->setSimplifiedMocsTableUsage(this->drm->getSimplifiedMocsTableUsage());
}

template <typename GfxFamily>
FlushStamp DrmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    DrmAllocation *alloc = static_cast<DrmAllocation *>(batchBuffer.commandBufferAllocation);
    DEBUG_BREAK_IF(!alloc);

    BufferObject *bb = alloc->getBO();
    FlushStamp flushStamp = 0;

    if (bb) {
        flushStamp = bb->peekHandle();
        unsigned int engineFlag = static_cast<OsContextLinux *>(osContext)->getEngineFlag();
        this->processResidency(allocationsForResidency);
        // Residency hold all allocation except command buffer, hence + 1
        auto requiredSize = this->residency.size() + 1;
        if (requiredSize > this->execObjectsStorage.size()) {
            this->execObjectsStorage.resize(requiredSize);
        }

        int err = bb->exec(static_cast<uint32_t>(alignUp(batchBuffer.usedSize - batchBuffer.startOffset, 8)),
                           batchBuffer.startOffset, engineFlag | I915_EXEC_NO_RELOC,
                           batchBuffer.requiresCoherency,
                           static_cast<OsContextLinux *>(osContext)->getDrmContextIds()[0],
                           this->residency.data(), this->residency.size(),
                           this->execObjectsStorage.data());
        UNRECOVERABLE_IF(err != 0);

        this->residency.clear();

        if (this->gemCloseWorkerOperationMode == gemCloseWorkerActive) {
            bb->reference();
            this->getMemoryManager()->peekGemCloseWorker()->push(bb);
        }
    }

    return flushStamp;
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::makeResident(GraphicsAllocation &gfxAllocation) {

    if (gfxAllocation.getUnderlyingBufferSize() == 0)
        return;

    CommandStreamReceiver::makeResident(gfxAllocation);
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
void DrmCommandStreamReceiver<GfxFamily>::processResidency(ResidencyContainer &inputAllocationsForResidency) {
    for (auto &alloc : inputAllocationsForResidency) {
        auto drmAlloc = static_cast<DrmAllocation *>(alloc);
        if (drmAlloc->fragmentsStorage.fragmentCount) {
            for (unsigned int f = 0; f < drmAlloc->fragmentsStorage.fragmentCount; f++) {
                const auto osContextId = osContext->getContextId();
                if (!drmAlloc->fragmentsStorage.fragmentStorageData[f].residency->resident[osContextId]) {
                    makeResident(drmAlloc->fragmentsStorage.fragmentStorageData[f].osHandleStorage->bo);
                    drmAlloc->fragmentsStorage.fragmentStorageData[f].residency->resident[osContextId] = true;
                }
            }
        } else {
            BufferObject *bo = drmAlloc->getBO();
            makeResident(bo);
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
        if (gfxAllocation.fragmentsStorage.fragmentCount) {
            for (auto fragmentId = 0u; fragmentId < gfxAllocation.fragmentsStorage.fragmentCount; fragmentId++) {
                gfxAllocation.fragmentsStorage.fragmentStorageData[fragmentId].residency->resident[osContext->getContextId()] = false;
            }
        }
    }
    gfxAllocation.releaseResidencyInOsContext(this->osContext->getContextId());
}

template <typename GfxFamily>
DrmMemoryManager *DrmCommandStreamReceiver<GfxFamily>::getMemoryManager() {
    return (DrmMemoryManager *)CommandStreamReceiver::getMemoryManager();
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
