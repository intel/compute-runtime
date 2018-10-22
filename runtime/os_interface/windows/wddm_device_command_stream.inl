/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Need to suppress warining 4005 caused by hw_cmds.h and wddm.h order.
// Current order must be preserved due to two versions of igfxfmid.h
#pragma warning(push)
#pragma warning(disable : 4005)
#include "hw_cmds.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/page_table_mngr.h"
#include "runtime/helpers/gmm_callbacks.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_device_command_stream.h"
#pragma warning(pop)

#undef max

#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"
namespace OCLRT {

// Initialize COMMAND_BUFFER_HEADER         Type PatchList  Streamer Perf Tag
DECLARE_COMMAND_BUFFER(CommandBufferHeader, UMD_OCL, FALSE, FALSE, PERFTAG_OCL);

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::WddmCommandStreamReceiver(const HardwareInfo &hwInfoIn,
                                                                ExecutionEnvironment &executionEnvironment)
    : BaseClass(hwInfoIn, executionEnvironment) {

    if (!executionEnvironment.osInterface) {
        executionEnvironment.osInterface = std::make_unique<OSInterface>();
        this->wddm = Wddm::createWddm();
        this->osInterface = executionEnvironment.osInterface.get();
        this->osInterface->get()->setWddm(this->wddm);
    } else {
        this->wddm = executionEnvironment.osInterface->get()->getWddm();
        this->osInterface = executionEnvironment.osInterface.get();
    }

    GPUNODE_ORDINAL nodeOrdinal = GPUNODE_3D;
    UNRECOVERABLE_IF(!WddmEngineMapper<GfxFamily>::engineNodeMap(hwInfoIn.capabilityTable.defaultEngineType, nodeOrdinal));
    this->wddm->setNode(nodeOrdinal);
    PreemptionMode preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfoIn);
    this->wddm->setPreemptionMode(preemptionMode);

    commandBufferHeader = new COMMAND_BUFFER_HEADER;
    *commandBufferHeader = CommandBufferHeader;

    if (preemptionMode != PreemptionMode::Disabled) {
        commandBufferHeader->NeedsMidBatchPreEmptionSupport = true;
    }

    this->dispatchMode = DispatchMode::BatchedDispatch;

    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }

    bool success = this->wddm->init();
    DEBUG_BREAK_IF(!success);
}

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::~WddmCommandStreamReceiver() {
    if (commandBufferHeader)
        delete commandBufferHeader;
}

template <typename GfxFamily>
FlushStamp WddmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer,
                                                       EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    auto commandStreamAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);

    if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
        makeResident(*batchBuffer.commandBufferAllocation);
    } else {
        allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);
        batchBuffer.commandBufferAllocation->residencyTaskCount[this->deviceIndex] = this->taskCount;
    }

    this->processResidency(allocationsForResidency, osContext);

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandBufferHeader);
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

    wddm->submit(commandStreamAddress, batchBuffer.usedSize - batchBuffer.startOffset, commandBufferHeader, *osContext.get());

    return osContext.get()->getResidencyController().getMonitoredFence().lastSubmittedFence;
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
void WddmCommandStreamReceiver<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    bool success = getMemoryManager()->makeResidentResidencyAllocations(allocationsForResidency, osContext);
    DEBUG_BREAK_IF(!success);
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::processEviction(OsContext &osContext) {
    getMemoryManager()->makeNonResidentEvictionAllocations(this->getEvictionAllocations(), osContext);
    this->getEvictionAllocations().clear();
}

template <typename GfxFamily>
WddmMemoryManager *WddmCommandStreamReceiver<GfxFamily>::getMemoryManager() {
    return static_cast<WddmMemoryManager *>(CommandStreamReceiver::getMemoryManager());
}

template <typename GfxFamily>
MemoryManager *WddmCommandStreamReceiver<GfxFamily>::createMemoryManager(bool enable64kbPages, bool enableLocalMemory) {
    return new WddmMemoryManager(enable64kbPages, enableLocalMemory, this->wddm, executionEnvironment);
}

template <typename GfxFamily>
bool WddmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStampToWait, OsContext &osContext) {
    return wddm->waitFromCpu(flushStampToWait, *osContext.get());
}

template <typename GfxFamily>
GmmPageTableMngr *WddmCommandStreamReceiver<GfxFamily>::createPageTableManager() {
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks = {};
    GMM_TRANSLATIONTABLE_CALLBACKS ttCallbacks = {};
    auto gdi = wddm->getGdi();

    // clang-format off
    deviceCallbacks.Adapter.KmtHandle         = wddm->getAdapter();
    deviceCallbacks.hDevice.KmtHandle         = wddm->getDevice();
    deviceCallbacks.hCsr            = static_cast<CommandStreamReceiverHw<GfxFamily> *>(this);
    deviceCallbacks.PagingQueue     = wddm->getPagingQueue();
    deviceCallbacks.PagingFence     = wddm->getPagingQueueSyncObject();

    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnAllocate     = gdi->createAllocation;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate   = gdi->destroyAllocation;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMapGPUVA     = gdi->mapGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMakeResident = gdi->makeResident;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEvict        = gdi->evict;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnReserveGPUVA = gdi->reserveGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUpdateGPUVA  = gdi->updateGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnWaitFromCpu  = gdi->waitForSynchronizationObjectFromCpu;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnLock         = gdi->lock2;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUnLock       = gdi->unlock2;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEscape       = gdi->escape;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture = DeviceCallbacks<GfxFamily>::notifyAubCapture;

    ttCallbacks.pfWriteL3Adr        = TTCallbacks<GfxFamily>::writeL3Address;
    // clang-format on

    GmmPageTableMngr *gmmPageTableMngr = GmmPageTableMngr::create(&deviceCallbacks, TT_TYPE::TRTT | TT_TYPE::AUXTT, &ttCallbacks);
    this->wddm->resetPageTableManager(gmmPageTableMngr);
    return gmmPageTableMngr;
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
void WddmCommandStreamReceiver<GfxFamily>::kmDafLockAllocations(ResidencyContainer &allocationsForResidency) {
    for (auto &graphicsAllocation : allocationsForResidency) {
        if ((GraphicsAllocation::AllocationType::LINEAR_STREAM == graphicsAllocation->getAllocationType()) ||
            (GraphicsAllocation::AllocationType::FILL_PATTERN == graphicsAllocation->getAllocationType())) {
            wddm->kmDafLock(static_cast<WddmAllocation *>(graphicsAllocation));
        }
    }
}
} // namespace OCLRT
