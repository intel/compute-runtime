/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

Gdi::Gdi() : gdiDll(createGdiDLL()) {
    if (gdiDll) {
        initialized = Gdi::getAllProcAddresses();
    }
    if constexpr (GdiLogging::gdiLoggingSupport) {
        GdiLogging::init();
    }
}

Gdi::~Gdi() {
    if constexpr (GdiLogging::gdiLoggingSupport) {
        GdiLogging::close();
    }
    this->profiler.printGdiTimes();
}

bool Gdi::setupHwQueueProcAddresses() {
    createHwQueue = gdiDll->getProcAddress("D3DKMTCreateHwQueue");
    destroyHwQueue = gdiDll->getProcAddress("D3DKMTDestroyHwQueue");
    submitCommandToHwQueue = gdiDll->getProcAddress("D3DKMTSubmitCommandToHwQueue");

    if (!createHwQueue || !destroyHwQueue || !submitCommandToHwQueue) {
        return false;
    }
    return true;
}

bool Gdi::getAllProcAddresses() {
    openAdapterFromLuid = gdiDll->getProcAddress("D3DKMTOpenAdapterFromLuid");
    createAllocation = gdiDll->getProcAddress("D3DKMTCreateAllocation");
    shareObjects = reinterpret_cast<decltype(shareObjects)>(gdiDll->getProcAddress("D3DKMTShareObjects"));
    createAllocation2 = gdiDll->getProcAddress("D3DKMTCreateAllocation2");
    destroyAllocation2 = gdiDll->getProcAddress("D3DKMTDestroyAllocation2");
    queryAdapterInfo = gdiDll->getProcAddress("D3DKMTQueryAdapterInfo");
    closeAdapter = gdiDll->getProcAddress("D3DKMTCloseAdapter");
    createDevice = gdiDll->getProcAddress("D3DKMTCreateDevice");
    destroyDevice = gdiDll->getProcAddress("D3DKMTDestroyDevice");
    escape = gdiDll->getProcAddress("D3DKMTEscape");
    createContext = gdiDll->getProcAddress("D3DKMTCreateContextVirtual");
    destroyContext = gdiDll->getProcAddress("D3DKMTDestroyContext");
    openResource = gdiDll->getProcAddress("D3DKMTOpenResource");
    openResourceFromNtHandle = gdiDll->getProcAddress("D3DKMTOpenResourceFromNtHandle");
    queryResourceInfo = gdiDll->getProcAddress("D3DKMTQueryResourceInfo");
    queryResourceInfoFromNtHandle = gdiDll->getProcAddress("D3DKMTQueryResourceInfoFromNtHandle");
    createSynchronizationObject = gdiDll->getProcAddress("D3DKMTCreateSynchronizationObject");
    createSynchronizationObject2 = gdiDll->getProcAddress("D3DKMTCreateSynchronizationObject2");
    destroySynchronizationObject = gdiDll->getProcAddress("D3DKMTDestroySynchronizationObject");
    createNativeFence = gdiDll->getProcAddress("D3DKMTCreateNativeFence");
    signalSynchronizationObject = gdiDll->getProcAddress("D3DKMTSignalSynchronizationObject");
    waitForSynchronizationObject = gdiDll->getProcAddress("D3DKMTWaitForSynchronizationObject");
    waitForSynchronizationObjectFromCpu = gdiDll->getProcAddress("D3DKMTWaitForSynchronizationObjectFromCpu");
    signalSynchronizationObjectFromCpu = gdiDll->getProcAddress("D3DKMTSignalSynchronizationObjectFromCpu");
    waitForSynchronizationObjectFromGpu = gdiDll->getProcAddress("D3DKMTWaitForSynchronizationObjectFromGpu");
    signalSynchronizationObjectFromGpu = gdiDll->getProcAddress("D3DKMTSignalSynchronizationObjectFromGpu");
    openSyncObjectFromNtHandle2 = gdiDll->getProcAddress("D3DKMTOpenSyncObjectFromNtHandle2");
    openSyncObjectNtHandleFromName = gdiDll->getProcAddress("D3DKMTOpenSyncObjectNtHandleFromName");
    createPagingQueue = gdiDll->getProcAddress("D3DKMTCreatePagingQueue");
    destroyPagingQueue = gdiDll->getProcAddress("D3DKMTDestroyPagingQueue");
    lock2 = gdiDll->getProcAddress("D3DKMTLock2");
    unlock2 = gdiDll->getProcAddress("D3DKMTUnlock2");
    mapGpuVirtualAddress = gdiDll->getProcAddress("D3DKMTMapGpuVirtualAddress");
    reserveGpuVirtualAddress = gdiDll->getProcAddress("D3DKMTReserveGpuVirtualAddress");
    freeGpuVirtualAddress = gdiDll->getProcAddress("D3DKMTFreeGpuVirtualAddress");
    updateGpuVirtualAddress = gdiDll->getProcAddress("D3DKMTUpdateGpuVirtualAddress");
    submitCommand = gdiDll->getProcAddress("D3DKMTSubmitCommand");
    makeResident = gdiDll->getProcAddress("D3DKMTMakeResident");
    evict = gdiDll->getProcAddress("D3DKMTEvict");
    registerTrimNotification = gdiDll->getProcAddress("D3DKMTRegisterTrimNotification");
    unregisterTrimNotification = gdiDll->getProcAddress("D3DKMTUnregisterTrimNotification");
    setAllocationPriority = gdiDll->getProcAddress("D3DKMTSetAllocationPriority");
    setSchedulingPriority = gdiDll->getProcAddress("D3DKMTSetContextSchedulingPriority");

    // For debug purposes
    getDeviceState = gdiDll->getProcAddress("D3DKMTGetDeviceState");

    // clang-format off
    if (openAdapterFromLuid && createAllocation2 
        && destroyAllocation2 && shareObjects && queryAdapterInfo && closeAdapter && createDevice
        && destroyDevice && escape && createContext && destroyContext 
        && openResource && queryResourceInfo
        && createSynchronizationObject && createSynchronizationObject2 
        && destroySynchronizationObject && signalSynchronizationObject
        && waitForSynchronizationObject && waitForSynchronizationObjectFromCpu
        && signalSynchronizationObjectFromCpu && waitForSynchronizationObjectFromGpu
        && signalSynchronizationObjectFromGpu && createPagingQueue && destroyPagingQueue
        && lock2 && unlock2 && mapGpuVirtualAddress && reserveGpuVirtualAddress
        && freeGpuVirtualAddress && updateGpuVirtualAddress &&submitCommand 
        && makeResident && evict && setSchedulingPriority){
        if (NEO::OSInterface::requiresSupportForWddmTrimNotification) {
            if(registerTrimNotification && unregisterTrimNotification){
                return true;
            }
        }else{
            return true;
        }
    }
    // clang-format on
    return false;
}
} // namespace NEO
