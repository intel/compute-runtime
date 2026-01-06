/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mock_gdi/mock_os_library.h"

#include "shared/test/common/mock_gdi/mock_gdi.h"

namespace NEO {
bool MockOsLibrary::isLoaded() {
    return true;
}

std::string MockOsLibrary::getFullPath() {
    return std::string();
}

void *MockOsLibrary::getProcAddress(const std::string &procName) {
    if (procName == "D3DKMTCreateAllocation") {
        return reinterpret_cast<void *>(mockD3DKMTCreateAllocation);
    }
    if (procName == "D3DKMTCreateAllocation2") {
        return reinterpret_cast<void *>(mockD3DKMTCreateAllocation2);
    }
    if (procName == "D3DKMTOpenAdapterFromLuid") {
        return reinterpret_cast<void *>(mockD3DKMTOpenAdapterFromLuid);
    }
    if (procName == "D3DKMTShareObjects") {
        return reinterpret_cast<void *>(mockD3DKMTShareObjects);
    }
    if (procName == "D3DKMTDestroyAllocation2") {
        return reinterpret_cast<void *>(mockD3DKMTDestroyAllocation2);
    }
    if (procName == "D3DKMTQueryAdapterInfo") {
        return reinterpret_cast<void *>(mockD3DKMTQueryAdapterInfo);
    }
    if (procName == "D3DKMTCreateDevice") {
        return reinterpret_cast<void *>(mockD3DKMTCreateDevice);
    }
    if (procName == "D3DKMTDestroyDevice") {
        return reinterpret_cast<void *>(mockD3DKMTDestroyDevice);
    }
    if (procName == "D3DKMTEscape") {
        return reinterpret_cast<void *>(mockD3DKMTEscape);
    }
    if (procName == "D3DKMTCreateContextVirtual") {
        return reinterpret_cast<void *>(mockD3DKMTCreateContextVirtual);
    }
    if (procName == "D3DKMTDestroyContext") {
        return reinterpret_cast<void *>(mockD3DKMTDestroyContext);
    }
    if (procName == "D3DKMTOpenResource") {
        return reinterpret_cast<void *>(mockD3DKMTOpenResource);
    }
    if (procName == "D3DKMTOpenResourceFromNtHandle") {
        return reinterpret_cast<void *>(mockD3DKMTOpenResourceFromNtHandle);
    }
    if (procName == "D3DKMTQueryResourceInfo") {
        return reinterpret_cast<void *>(mockD3DKMTQueryResourceInfo);
    }
    if (procName == "D3DKMTQueryResourceInfoFromNtHandle") {
        return reinterpret_cast<void *>(mockD3DKMTQueryResourceInfoFromNtHandle);
    }
    if (procName == "D3DKMTCreateSynchronizationObject2") {
        return reinterpret_cast<void *>(mockD3DKMTCreateSynchronizationObject2);
    }
    if (procName == "D3DKMTCreateNativeFence") {
        return reinterpret_cast<void *>(mockD3DKMTCreateNativeFence);
    }
    if (procName == "D3DKMTDestroySynchronizationObject") {
        return reinterpret_cast<void *>(mockD3DKMTDestroySynchronizationObject);
    }
    if (procName == "D3DKMTCreatePagingQueue") {
        return reinterpret_cast<void *>(mockD3DKMTCreatePagingQueue);
    }
    if (procName == "D3DKMTDestroyPagingQueue") {
        return reinterpret_cast<void *>(mockD3DKMTDestroyPagingQueue);
    }
    if (procName == "D3DKMTLock2") {
        return reinterpret_cast<void *>(mockD3DKMTLock2);
    }
    if (procName == "D3DKMTUnlock2") {
        return reinterpret_cast<void *>(mockD3DKMTUnlock2);
    }
    if (procName == "D3DKMTMapGpuVirtualAddress") {
        return reinterpret_cast<void *>(mockD3DKMTMapGpuVirtualAddress);
    }
    if (procName == "D3DKMTReserveGpuVirtualAddress") {
        return reinterpret_cast<void *>(mockD3DKMTReserveGpuVirtualAddress);
    }
    if (procName == "D3DKMTSubmitCommandToHwQueue") {
        return reinterpret_cast<void *>(mockD3DKMTSubmitCommandToHwQueue);
    }
    if (procName == "D3DKMTMakeResident") {
        return reinterpret_cast<void *>(mockD3DKMTMakeResident);
    }
    if (procName == "D3DKMTRegisterTrimNotification") {
        return reinterpret_cast<void *>(mockD3DKMTRegisterTrimNotification);
    }
    if (procName == "D3DKMTSetAllocationPriority") {
        return reinterpret_cast<void *>(mockD3DKMTSetAllocationPriority);
    }
    if (procName == "D3DKMTSetContextSchedulingPriority") {
        return reinterpret_cast<void *>(mockD3DKMTSetContextSchedulingPriority);
    }
    if (procName == "D3DKMTCreateHwQueue") {
        return reinterpret_cast<void *>(mockD3DKMTCreateHwQueue);
    }
    if (procName == "D3DKMTDestroyHwQueue") {
        return reinterpret_cast<void *>(mockD3DKMTDestroyHwQueue);
    }
    if (procName == "mockSetAdapterInfo") {
        return reinterpret_cast<void *>(mockSetAdapterInfo);
    }
    if (procName == "setMockSizes") {
        return reinterpret_cast<void *>(setMockSizes);
    }
    if (procName == "getMockSizes") {
        return reinterpret_cast<void *>(getMockSizes);
    }
    if (procName == "getMockLastDestroyedResHandle") {
        return reinterpret_cast<void *>(getMockLastDestroyedResHandle);
    }
    if (procName == "setMockLastDestroyedResHandle") {
        return reinterpret_cast<void *>(setMockLastDestroyedResHandle);
    }
    if (procName == "getMockCreateDeviceParams") {
        return reinterpret_cast<void *>(getMockCreateDeviceParams);
    }
    if (procName == "setMockCreateDeviceParams") {
        return reinterpret_cast<void *>(setMockCreateDeviceParams);
    }
    if (procName == "setMockDeviceExecutionState") {
        return reinterpret_cast<void *>(setMockDeviceExecutionState);
    }
    if (procName == "setMockGetDeviceStateReturnValue") {
        return reinterpret_cast<void *>(setMockGetDeviceStateReturnValue);
    }
    if (procName == "getMockAllocation") {
        return reinterpret_cast<void *>(getMockAllocation);
    }
    if (procName == "getAdapterInfoAddress") {
        return reinterpret_cast<void *>(getAdapterInfoAddress);
    }
    if (procName == "getLastCallMapGpuVaArg") {
        return reinterpret_cast<void *>(getLastCallMapGpuVaArg);
    }
    if (procName == "getLastCallReserveGpuVaArg") {
        return reinterpret_cast<void *>(getLastCallReserveGpuVaArg);
    }
    if (procName == "setMapGpuVaFailConfig") {
        return reinterpret_cast<void *>(setMapGpuVaFailConfig);
    }
    if (procName == "getCreateContextData") {
        return reinterpret_cast<void *>(getCreateContextData);
    }
    if (procName == "getCreateHwQueueData") {
        return reinterpret_cast<void *>(getCreateHwQueueData);
    }
    if (procName == "getDestroyHwQueueData") {
        return reinterpret_cast<void *>(getDestroyHwQueueData);
    }
    if (procName == "getSubmitCommandToHwQueueData") {
        return reinterpret_cast<void *>(getSubmitCommandToHwQueueData);
    }
    if (procName == "getDestroySynchronizationObjectData") {
        return reinterpret_cast<void *>(getDestroySynchronizationObjectData);
    }
    if (procName == "getMonitorFenceCpuFenceAddress") {
        return reinterpret_cast<void *>(getMonitorFenceCpuFenceAddress);
    }
    if (procName == "getMonitorFenceCpuAddressSelector") {
        return reinterpret_cast<void *>(getMonitorFenceCpuAddressSelector);
    }
    if (procName == "getCreateSynchronizationObject2FailCall") {
        return reinterpret_cast<void *>(getCreateSynchronizationObject2FailCall);
    }
    if (procName == "getFailOnSetContextSchedulingPriorityCall") {
        return reinterpret_cast<void *>(getFailOnSetContextSchedulingPriorityCall);
    }
    if (procName == "getSetContextSchedulingPriorityDataCall") {
        return reinterpret_cast<void *>(getSetContextSchedulingPriorityDataCall);
    }
    if (procName == "getRegisterTrimNotificationFailCall") {
        return reinterpret_cast<void *>(getRegisterTrimNotificationFailCall);
    }
    if (procName == "getLastPriority") {
        return reinterpret_cast<void *>(getLastPriority);
    }
    if (procName == "setAdapterBDF") {
        return reinterpret_cast<void *>(setAdapterBDF);
    }
    if (procName == "D3DKMTOpenAdapterFromHdc") {
        return reinterpret_cast<void *>(mockD3DKMTOpenAdapterFromHdc);
    }
    if (procName == "D3DKMTDestroyAllocation") {
        return reinterpret_cast<void *>(mockD3DKMTDestroyAllocation);
    }
    if (procName == "D3DKMTCloseAdapter") {
        return reinterpret_cast<void *>(mockD3DKMTCloseAdapter);
    }
    if (procName == "D3DKMTLock") {
        return reinterpret_cast<void *>(mockD3DKMTLock);
    }
    if (procName == "D3DKMTUnlock") {
        return reinterpret_cast<void *>(mockD3DKMTUnlock);
    }
    if (procName == "D3DKMTRender") {
        return reinterpret_cast<void *>(mockD3DKMTRender);
    }
    if (procName == "D3DKMTCreateSynchronizationObject") {
        return reinterpret_cast<void *>(mockD3DKMTCreateSynchronizationObject);
    }
    if (procName == "D3DKMTSignalSynchronizationObject") {
        return reinterpret_cast<void *>(mockD3DKMTSignalSynchronizationObject);
    }
    if (procName == "D3DKMTWaitForSynchronizationObject") {
        return reinterpret_cast<void *>(mockD3DKMTWaitForSynchronizationObject);
    }
    if (procName == "D3DKMTWaitForSynchronizationObjectFromCpu") {
        return reinterpret_cast<void *>(mockD3DKMTWaitForSynchronizationObjectFromCpu);
    }
    if (procName == "D3DKMTSignalSynchronizationObjectFromCpu") {
        return reinterpret_cast<void *>(mockD3DKMTSignalSynchronizationObjectFromCpu);
    }
    if (procName == "D3DKMTWaitForSynchronizationObjectFromGpu") {
        return reinterpret_cast<void *>(mockD3DKMTWaitForSynchronizationObjectFromGpu);
    }
    if (procName == "D3DKMTSignalSynchronizationObjectFromGpu") {
        return reinterpret_cast<void *>(mockD3DKMTSignalSynchronizationObjectFromGpu);
    }
    if (procName == "D3DKMTOpenSyncObjectFromNtHandle2") {
        return reinterpret_cast<void *>(mockD3DKMTOpenSyncObjectFromNtHandle2);
    }
    if (procName == "D3DKMTOpenSyncObjectNtHandleFromName") {
        return reinterpret_cast<void *>(mockD3DKMTOpenSyncObjectNtHandleFromName);
    }
    if (procName == "D3DKMTFreeGpuVirtualAddress") {
        return reinterpret_cast<void *>(mockD3DKMTFreeGpuVirtualAddress);
    }
    if (procName == "D3DKMTUpdateGpuVirtualAddress") {
        return reinterpret_cast<void *>(mockD3DKMTUpdateGpuVirtualAddress);
    }
    if (procName == "D3DKMTSubmitCommand") {
        return reinterpret_cast<void *>(mockD3DKMTSubmitCommand);
    }
    if (procName == "D3DKMTEvict") {
        return reinterpret_cast<void *>(mockD3DKMTEvict);
    }
    if (procName == "D3DKMTGetDeviceState") {
        return reinterpret_cast<void *>(mockD3DKMTGetDeviceState);
    }
    if (procName == "D3DKMTUnregisterTrimNotification") {
        return reinterpret_cast<void *>(mockD3DKMTUnregisterTrimNotification);
    }
    if (procName == "setCapturingCreateAllocationFlags") {
        return reinterpret_cast<void *>(setCapturingCreateAllocationFlags);
    }
    if (procName == "getCapturedCreateAllocationFlags") {
        return reinterpret_cast<void *>(getCapturedCreateAllocationFlags);
    }
    if (procName == "setSupportCreateAllocationWithReadWriteExisitingSysMemory") {
        return reinterpret_cast<void *>(setSupportCreateAllocationWithReadWriteExisitingSysMemory);
    }
    return nullptr;
}

} // namespace NEO
