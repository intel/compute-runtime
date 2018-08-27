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

#include "runtime/command_stream/linear_stream.h"
#include "hw_cmds.h"
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
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/platform/platform.h"
#include <cstdlib>
#include <cstring>

namespace OCLRT {

template <typename GfxFamily>
DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver(const HardwareInfo &hwInfoIn,
                                                              Drm *drm, ExecutionEnvironment &executionEnvironment, gemCloseWorkerMode mode)
    : BaseClass(hwInfoIn, executionEnvironment), gemCloseWorkerOperationMode(mode) {
    this->drm = drm ? drm : Drm::get(0);
    residency.reserve(512);
    execObjectsStorage.reserve(512);
    if (!executionEnvironment.osInterface) {
        executionEnvironment.osInterface = std::make_unique<OSInterface>();
    }
    executionEnvironment.osInterface->get()->setDrm(this->drm);
    CommandStreamReceiver::osInterface = executionEnvironment.osInterface.get();
    auto gmmHelper = platform()->peekExecutionEnvironment()->getGmmHelper();
    gmmHelper->setSimplifiedMocsTableUsage(this->drm->getSimplifiedMocsTableUsage());
}

template <typename GfxFamily>
FlushStamp DrmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency, OsContext &osContext) {
    unsigned int engineFlag = 0xFF;
    bool ret = DrmEngineMapper<GfxFamily>::engineNodeMap(engineType, engineFlag);
    UNRECOVERABLE_IF(!(ret));

    DrmAllocation *alloc = static_cast<DrmAllocation *>(batchBuffer.commandBufferAllocation);
    DEBUG_BREAK_IF(!alloc);

    size_t alignedStart = (reinterpret_cast<uintptr_t>(batchBuffer.commandBufferAllocation->getUnderlyingBuffer()) & (MemoryConstants::allocationAlignment - 1)) + batchBuffer.startOffset;
    BufferObject *bb = alloc->getBO();
    FlushStamp flushStamp = 0;

    if (bb) {
        flushStamp = bb->peekHandle();
        this->processResidency(allocationsForResidency, osContext);
        // Residency hold all allocation except command buffer, hence + 1
        auto requiredSize = this->residency.size() + 1;
        if (requiredSize > this->execObjectsStorage.size()) {
            this->execObjectsStorage.resize(requiredSize);
        }

        bb->swapResidencyVector(&this->residency);
        bb->setExecObjectsStorage(this->execObjectsStorage.data());
        this->residency.reserve(512);

        bb->exec(static_cast<uint32_t>(alignUp(batchBuffer.usedSize - batchBuffer.startOffset, 8)),
                 alignedStart, engineFlag | I915_EXEC_NO_RELOC,
                 batchBuffer.requiresCoherency,
                 batchBuffer.low_priority);

        for (auto &surface : *(bb->getResidency())) {
            surface->setIsResident(false);
        }
        bb->getResidency()->clear();

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
    if (bo && !bo->peekIsResident()) {
        bo->setIsResident(true);
        residency.push_back(bo);
    }
}

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::processResidency(ResidencyContainer *inputAllocationsForResidency, OsContext &osContext) {
    auto &allocationsForResidency = inputAllocationsForResidency ? *inputAllocationsForResidency : getMemoryManager()->getResidencyAllocations();
    for (uint32_t a = 0; a < allocationsForResidency.size(); a++) {
        DrmAllocation *drmAlloc = reinterpret_cast<DrmAllocation *>(allocationsForResidency[a]);
        if (drmAlloc->fragmentsStorage.fragmentCount) {
            for (unsigned int f = 0; f < drmAlloc->fragmentsStorage.fragmentCount; f++) {
                makeResident(drmAlloc->fragmentsStorage.fragmentStorageData[f].osHandleStorage->bo);
                drmAlloc->fragmentsStorage.fragmentStorageData[f].residency->resident = true;
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
    if (gfxAllocation.residencyTaskCount != ObjectNotResident) {
        for (auto &surface : residency) {
            surface->setIsResident(false);
        }
        if (this->residency.size() != 0) {
            this->residency.clear();
        }
        if (gfxAllocation.fragmentsStorage.fragmentCount) {
            for (auto fragmentId = 0u; fragmentId < gfxAllocation.fragmentsStorage.fragmentCount; fragmentId++) {
                gfxAllocation.fragmentsStorage.fragmentStorageData[fragmentId].osHandleStorage->bo->setIsResident(false);
                gfxAllocation.fragmentsStorage.fragmentStorageData[fragmentId].residency->resident = false;
            }
        }
    }
    gfxAllocation.residencyTaskCount = ObjectNotResident;
}

template <typename GfxFamily>
DrmMemoryManager *DrmCommandStreamReceiver<GfxFamily>::getMemoryManager() {
    return (DrmMemoryManager *)CommandStreamReceiver::getMemoryManager();
}

template <typename GfxFamily>
MemoryManager *DrmCommandStreamReceiver<GfxFamily>::createMemoryManager(bool enable64kbPages) {
    memoryManager = new DrmMemoryManager(this->drm, this->gemCloseWorkerOperationMode, DebugManager.flags.EnableForcePin.get(), true);
    return memoryManager;
}

template <typename GfxFamily>
bool DrmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStamp, OsContext &osContext) {
    drm_i915_gem_wait wait = {};
    wait.bo_handle = static_cast<uint32_t>(flushStamp);
    wait.timeout_ns = -1;

    drm->ioctl(DRM_IOCTL_I915_GEM_WAIT, &wait);
    return true;
}

template <typename GfxFamily>
inline void DrmCommandStreamReceiver<GfxFamily>::overrideMediaVFEStateDirty(bool dirty) {
    this->mediaVfeStateDirty = dirty;
    this->mediaVfeStateLowPriorityDirty = dirty;
}

template <typename GfxFamily>
inline void DrmCommandStreamReceiver<GfxFamily>::programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags) {
    bool &currentContextDirtyFlag = dispatchFlags.lowPriority ? mediaVfeStateLowPriorityDirty : mediaVfeStateDirty;

    if (currentContextDirtyFlag) {
        PreambleHelper<GfxFamily>::programVFEState(&csr, hwInfo, requiredScratchSize, getScratchPatchAddress());
        currentContextDirtyFlag = false;
    }
}

} // namespace OCLRT
