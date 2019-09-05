/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Need to suppress warining 4005 caused by hw_cmds.h and wddm.h order.
// Current order must be preserved due to two versions of igfxfmid.h
#pragma warning(push)
#pragma warning(disable : 4005)
#include "core/command_stream/linear_stream.h"
#include "core/helpers/ptr_math.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/page_table_mngr.h"
#include "runtime/helpers/gmm_callbacks.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_device_command_stream.h"

#include "hw_cmds.h"
#pragma warning(pop)

#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"
namespace NEO {

// Initialize COMMAND_BUFFER_HEADER         Type PatchList  Streamer Perf Tag
DECLARE_COMMAND_BUFFER(CommandBufferHeader, UMD_OCL, FALSE, FALSE, PERFTAG_OCL);

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::WddmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment)
    : BaseClass(executionEnvironment) {

    notifyAubCaptureImpl = DeviceCallbacks<GfxFamily>::notifyAubCapture;
    this->wddm = executionEnvironment.osInterface->get()->getWddm();
    this->osInterface = executionEnvironment.osInterface.get();

    PreemptionMode preemptionMode = PreemptionHelper::getDefaultPreemptionMode(peekHwInfo());

    commandBufferHeader = new COMMAND_BUFFER_HEADER;
    *commandBufferHeader = CommandBufferHeader;

    if (preemptionMode != PreemptionMode::Disabled) {
        commandBufferHeader->NeedsMidBatchPreEmptionSupport = true;
    }

    this->dispatchMode = DispatchMode::BatchedDispatch;

    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }
}

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::~WddmCommandStreamReceiver() {
    if (commandBufferHeader)
        delete commandBufferHeader;
}

template <typename GfxFamily>
FlushStamp WddmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    auto commandStreamAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);

    if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
        makeResident(*batchBuffer.commandBufferAllocation);
    } else {
        allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);
        batchBuffer.commandBufferAllocation->updateResidencyTaskCount(this->taskCount, this->osContext->getContextId());
    }

    this->processResidency(allocationsForResidency);

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandBufferHeader);
    pHeader->RequiresCoherency = batchBuffer.requiresCoherency;

    pHeader->UmdRequestedSliceState = 0;
    pHeader->UmdRequestedEUCount = wddm->getGtSysInfo()->EUCount / wddm->getGtSysInfo()->SubSliceCount;

    const uint32_t maxRequestedSubsliceCount = 7;
    switch (batchBuffer.throttle) {
    case QueueThrottle::LOW:
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

    auto osContextWin = static_cast<OsContextWin *>(osContext);
    wddm->submit(commandStreamAddress, batchBuffer.usedSize - batchBuffer.startOffset, commandBufferHeader, *osContextWin);

    return osContextWin->getResidencyController().getMonitoredFence().lastSubmittedFence;
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::makeResident(GraphicsAllocation &gfxAllocation) {
    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation =", &gfxAllocation);

    if (gfxAllocation.fragmentsStorage.fragmentCount == 0) {
        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation default handle =", static_cast<WddmAllocation &>(gfxAllocation).getDefaultHandle());
    } else {
        for (uint32_t allocationId = 0; allocationId < gfxAllocation.fragmentsStorage.fragmentCount; allocationId++) {
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "fragment handle =", gfxAllocation.fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle);
        }
    }

    CommandStreamReceiver::makeResident(gfxAllocation);
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::processResidency(const ResidencyContainer &allocationsForResidency) {
    bool success = static_cast<OsContextWin *>(osContext)->getResidencyController().makeResidentResidencyAllocations(allocationsForResidency);
    DEBUG_BREAK_IF(!success);
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::processEviction() {
    static_cast<OsContextWin *>(osContext)->getResidencyController().makeNonResidentEvictionAllocations(this->getEvictionAllocations());
    this->getEvictionAllocations().clear();
}

template <typename GfxFamily>
WddmMemoryManager *WddmCommandStreamReceiver<GfxFamily>::getMemoryManager() const {
    return static_cast<WddmMemoryManager *>(CommandStreamReceiver::getMemoryManager());
}

template <typename GfxFamily>
bool WddmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStampToWait) {
    return wddm->waitFromCpu(flushStampToWait, static_cast<OsContextWin *>(osContext)->getResidencyController().getMonitoredFence());
}

template <typename GfxFamily>
GmmPageTableMngr *WddmCommandStreamReceiver<GfxFamily>::createPageTableManager() {
    GMM_TRANSLATIONTABLE_CALLBACKS ttCallbacks = {};
    ttCallbacks.pfWriteL3Adr = TTCallbacks<GfxFamily>::writeL3Address;

    GmmPageTableMngr *gmmPageTableMngr = GmmPageTableMngr::create(TT_TYPE::TRTT | TT_TYPE::AUXTT, &ttCallbacks);
    gmmPageTableMngr->setCsrHandle(this);
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
            (GraphicsAllocation::AllocationType::FILL_PATTERN == graphicsAllocation->getAllocationType()) ||
            (GraphicsAllocation::AllocationType::COMMAND_BUFFER == graphicsAllocation->getAllocationType())) {
            wddm->kmDafLock(static_cast<WddmAllocation *>(graphicsAllocation)->getDefaultHandle());
        }
    }
}
} // namespace NEO
