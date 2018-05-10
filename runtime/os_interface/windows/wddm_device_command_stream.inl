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

// Need to suppress warining 4005 caused by hw_cmds.h and wddm.h order.
// Current order must be preserved due to two versions of igfxfmid.h
#pragma warning(push)
#pragma warning(disable : 4005)
#include "hw_cmds.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/device/device.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_device_command_stream.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/translationtable_callbacks.h"
#include "runtime/gmm_helper/page_table_mngr.h"
#pragma warning(pop)

#undef max

#include "runtime/os_interface/windows/wddm_memory_manager.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/gdi_interface.h"
namespace OCLRT {

// Initialize COMMAND_BUFFER_HEADER         Type PatchList  Streamer Perf Tag
DECLARE_COMMAND_BUFFER(CommandBufferHeader, UMD_OCL, FALSE, FALSE, PERFTAG_OCL);

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::WddmCommandStreamReceiver(const HardwareInfo &hwInfoIn, Wddm *wddm)
    : BaseClass(hwInfoIn) {
    this->wddm = wddm;
    if (this->wddm == nullptr) {
        this->wddm = Wddm::createWddm();
    }
    GPUNODE_ORDINAL nodeOrdinal = GPUNODE_3D;
    UNRECOVERABLE_IF(!WddmEngineMapper<GfxFamily>::engineNodeMap(hwInfoIn.capabilityTable.defaultEngineType, nodeOrdinal));
    this->wddm->setNode(nodeOrdinal);
    PreemptionMode preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfoIn);
    this->wddm->setPreemptionMode(preemptionMode);
    this->osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    this->osInterface.get()->get()->setWddm(this->wddm);
    commandBufferHeader = new COMMAND_BUFFER_HEADER;
    *commandBufferHeader = CommandBufferHeader;
    this->dispatchMode = DispatchMode::BatchedDispatch;

    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }

    bool success = this->wddm->init<GfxFamily>();
    DEBUG_BREAK_IF(!success);

    if (hwInfoIn.capabilityTable.ftrCompression) {
        this->wddm->resetPageTableManager(createPageTableManager());
    }
}

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::~WddmCommandStreamReceiver() {
    this->cleanupResources();

    if (commandBufferHeader)
        delete commandBufferHeader;
}

template <typename GfxFamily>
FlushStamp WddmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer,
                                                       EngineType engineType, ResidencyContainer *allocationsForResidency) {
    auto commandStreamAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);

    if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
        makeResident(*batchBuffer.commandBufferAllocation);
    } else {
        allocationsForResidency->push_back(batchBuffer.commandBufferAllocation);
        batchBuffer.commandBufferAllocation->residencyTaskCount = this->taskCount;
    }

    this->processResidency(allocationsForResidency);

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandBufferHeader);
    if (memoryManager->device->getPreemptionMode() != PreemptionMode::Disabled) {
        pHeader->NeedsMidBatchPreEmptionSupport = 1u;
    } else {
        pHeader->NeedsMidBatchPreEmptionSupport = 0u;
    }
    pHeader->RequiresCoherency = batchBuffer.requiresCoherency;

    pHeader->UmdRequestedSliceState = 0;
    pHeader->UmdRequestedEUCount = wddm->getGtSysInfo()->EUCount / wddm->getGtSysInfo()->SubSliceCount;

    const uint32_t maxRequestedSubsliceCount = 7;
    switch (batchBuffer.throttle) {
    case QueueThrottle::LOW:
        pHeader->UmdRequestedSubsliceCount = 1;
        break;
    case QueueThrottle::MEDIUM:
        pHeader->UmdRequestedSubsliceCount = 0;
        break;
    case QueueThrottle::HIGH:
        pHeader->UmdRequestedSubsliceCount = (wddm->getGtSysInfo()->SubSliceCount <= maxRequestedSubsliceCount) ? wddm->getGtSysInfo()->SubSliceCount : 0;
        break;
    }

    if (wddm->isKmDafEnabled()) {
        this->kmDafLockAllocations(allocationsForResidency);
    }

    wddm->submit(commandStreamAddress, batchBuffer.usedSize - batchBuffer.startOffset, commandBufferHeader);

    return wddm->getMonitoredFence().lastSubmittedFence;
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::makeResident(GraphicsAllocation &gfxAllocation) {
    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation =", reinterpret_cast<WddmAllocation *>(&gfxAllocation));

    if (gfxAllocation.fragmentsStorage.fragmentCount == 0) {
        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation handle =", reinterpret_cast<WddmAllocation *>(&gfxAllocation)->handle);
    } else {
        for (uint32_t allocationId = 0; allocationId < reinterpret_cast<WddmAllocation *>(&gfxAllocation)->fragmentsStorage.fragmentCount; allocationId++) {
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "fragment handle =", reinterpret_cast<WddmAllocation *>(&gfxAllocation)->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle);
        }
    }

    CommandStreamReceiver::makeResident(gfxAllocation);
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::processResidency(ResidencyContainer *allocationsForResidency) {
    bool success = getMemoryManager()->makeResidentResidencyAllocations(allocationsForResidency);
    DEBUG_BREAK_IF(!success);
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::processEviction() {
    getMemoryManager()->makeNonResidentEvictionAllocations();
    getMemoryManager()->clearEvictionAllocations();
}

template <typename GfxFamily>
WddmMemoryManager *WddmCommandStreamReceiver<GfxFamily>::getMemoryManager() {
    return (WddmMemoryManager *)CommandStreamReceiver::getMemoryManager();
}

template <typename GfxFamily>
MemoryManager *WddmCommandStreamReceiver<GfxFamily>::createMemoryManager(bool enable64kbPages) {
    return memoryManager = new WddmMemoryManager(enable64kbPages, this->wddm);
}

template <typename GfxFamily>
bool WddmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStampToWait) {
    return wddm->waitFromCpu(flushStampToWait);
}

template <typename GfxFamily>
GmmPageTableMngr *WddmCommandStreamReceiver<GfxFamily>::createPageTableManager() {
    GMM_DEVICE_CALLBACKS deviceCallbacks = {};
    GMM_TRANSLATIONTABLE_CALLBACKS ttCallbacks = {};
    auto gdi = wddm->getGdi();

    // clang-format off
    deviceCallbacks.Adapter         = wddm->getAdapter();
    deviceCallbacks.hDevice         = wddm->getDevice();
    deviceCallbacks.PagingQueue     = wddm->getPagingQueue();
    deviceCallbacks.PagingFence     = wddm->getPagingQueueSyncObject();

    deviceCallbacks.pfnAllocate     = gdi->createAllocation;
    deviceCallbacks.pfnDeallocate   = gdi->destroyAllocation;
    deviceCallbacks.pfnMapGPUVA     = gdi->mapGpuVirtualAddress;
    deviceCallbacks.pfnMakeResident = gdi->makeResident;
    deviceCallbacks.pfnEvict        = gdi->evict;
    deviceCallbacks.pfnReserveGPUVA = gdi->reserveGpuVirtualAddress;
    deviceCallbacks.pfnUpdateGPUVA  = gdi->updateGpuVirtualAddress;
    deviceCallbacks.pfnWaitFromCpu  = gdi->waitForSynchronizationObjectFromCpu;
    deviceCallbacks.pfnLock         = gdi->lock2;
    deviceCallbacks.pfnUnLock       = gdi->unlock2;
    deviceCallbacks.pfnEscape       = gdi->escape;

    ttCallbacks.pfWriteL3Adr        = TTCallbacks<GfxFamily>::writeL3Address;
    // clang-format on

    return GmmPageTableMngr::create(&deviceCallbacks, TT_TYPE::TRTT | TT_TYPE::AUXTT, &ttCallbacks);
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::initPageTableManagerRegisters(LinearStream &csr) {
    if (wddm->getPageTableManager() && !pageTableManagerInitialized) {
        wddm->getPageTableManager()->initContextTRTableRegister(this, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS);
        wddm->getPageTableManager()->initContextAuxTableRegister(this, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS);

        pageTableManagerInitialized = true;
    }
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::kmDafLockAllocations(ResidencyContainer *allocationsForResidency) {
    auto &residencyAllocations = allocationsForResidency ? *allocationsForResidency : getMemoryManager()->getResidencyAllocations();

    for (uint32_t i = 0; i < residencyAllocations.size(); i++) {
        auto graphicsAllocation = residencyAllocations[i];
        if ((GraphicsAllocation::ALLOCATION_TYPE_LINEAR_STREAM == graphicsAllocation->getAllocationType()) ||
            (GraphicsAllocation::ALLOCATION_TYPE_FILL_PATTERN == graphicsAllocation->getAllocationType())) {
            wddm->kmDafLock(static_cast<WddmAllocation *>(graphicsAllocation));
        }
    }
}
} // namespace OCLRT
