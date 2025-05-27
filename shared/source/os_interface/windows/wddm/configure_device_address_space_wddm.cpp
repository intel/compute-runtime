/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "gmm_memory.h"

namespace NEO {

long __stdcall notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate) {
    return notifyAubCaptureImpl(csrHandle, gfxAddress, gfxSize, allocate);
}

bool Wddm::configureDeviceAddressSpace() {
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
    deviceCallbacks.Adapter.KmtHandle = getAdapter();
    deviceCallbacks.hCsr = nullptr;
    deviceCallbacks.hDevice.KmtHandle = device;
    deviceCallbacks.PagingQueue = pagingQueue;
    deviceCallbacks.PagingFence = pagingQueueSyncObject;

    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnAllocate = getGdi()->createAllocation;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate = getGdi()->destroyAllocation;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate2 = getGdi()->destroyAllocation2;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMapGPUVA = getGdi()->mapGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMakeResident = getGdi()->makeResident;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEvict = getGdi()->evict;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnReserveGPUVA = getGdi()->reserveGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUpdateGPUVA = getGdi()->updateGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnWaitFromCpu = getGdi()->waitForSynchronizationObjectFromCpu;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnLock = getGdi()->lock2;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUnLock = getGdi()->unlock2;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEscape = getGdi()->escape;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnFreeGPUVA = getGdi()->freeGpuVirtualAddress;
    deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture = notifyAubCapture;

    GMM_DEVICE_INFO deviceInfo{};
    deviceInfo.pGfxPartition = &gfxPartition;
    deviceInfo.pDeviceCb = &deviceCallbacks;
    memcpy_s(deviceInfo.MsSegId, sizeof(deviceInfo.MsSegId), segmentId, sizeof(segmentId));
    deviceInfo.AdapterLocalMemory = dedicatedVideoMemory;
    deviceInfo.AdapterCpuVisibleLocalMemory = lmemBarSize;
    if (!gmmMemory->setDeviceInfo(&deviceInfo)) {
        return false;
    }
    SYSTEM_INFO sysInfo;
    Wddm::getSystemInfo(&sysInfo);
    maximumApplicationAddress = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress);
    auto productFamily = gfxPlatform->eProductFamily;
    if (!hardwareInfoTable[productFamily]) {
        return false;
    }
    auto svmSize = hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                       ? maximumApplicationAddress + 1u
                       : 0u;

    bool obtainMinAddress = rootDeviceEnvironment.getHardwareInfo()->platform.eRenderCoreFamily == IGFX_GEN12LP_CORE;
    return gmmMemory->configureDevice(getAdapter(), device, getGdi()->escape, svmSize, featureTable->flags.ftrL3IACoherency, minAddress, obtainMinAddress);
}

} // namespace NEO
