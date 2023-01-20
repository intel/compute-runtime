/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mock_gdi/mock_gdi.h"

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"

ADAPTER_INFO_KMD gAdapterInfo{};
D3DDDI_MAPGPUVIRTUALADDRESS gLastCallMapGpuVaArg{};
D3DDDI_RESERVEGPUVIRTUALADDRESS gLastCallReserveGpuVaArg{};
uint32_t gMapGpuVaFailConfigCount = 0;
uint32_t gMapGpuVaFailConfigMax = 0;
uint64_t gGpuAddressSpace = 0ull;
uint32_t gLastPriority = 0ull;
ADAPTER_BDF gAdapterBDF{};

NTSTATUS __stdcall mockD3DKMTEscape(IN CONST D3DKMT_ESCAPE *pData) {
    static int perfTicks = 0;
    ++perfTicks;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.RetCode = NEO::GTDI_RET_OK;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.gpuPerfTicks = ++perfTicks;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.cpuPerfTicks = perfTicks;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.gpuPerfFreq = 1;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.cpuPerfFreq = 1;

    return STATUS_SUCCESS;
}

UINT64 PagingFence = 0;

void mockSetAdapterInfo(const void *pGfxPlatform, const void *pGTSystemInfo, uint64_t gpuAddressSpace) {
    if (pGfxPlatform != NULL) {
        gAdapterInfo.GfxPlatform = *(PLATFORM *)pGfxPlatform;
    }
    if (pGTSystemInfo != NULL) {
        gAdapterInfo.SystemInfo = *(GT_SYSTEM_INFO *)pGTSystemInfo;
    }
    gGpuAddressSpace = gpuAddressSpace;
    initGfxPartition();
}

NTSTATUS __stdcall mockD3DKMTOpenAdapterFromLuid(IN OUT CONST D3DKMT_OPENADAPTERFROMLUID *openAdapter) {
    if (openAdapter == nullptr || (openAdapter->AdapterLuid.HighPart == 0 && openAdapter->AdapterLuid.LowPart == 0)) {
        return STATUS_INVALID_PARAMETER;
    }
    D3DKMT_OPENADAPTERFROMLUID *openAdapterNonConst = const_cast<D3DKMT_OPENADAPTERFROMLUID *>(openAdapter);
    if (openAdapter->AdapterLuid.HighPart == 0xdd) {
        openAdapterNonConst->hAdapter = SHADOW_ADAPTER_HANDLE;
    } else {
        openAdapterNonConst->hAdapter = ADAPTER_HANDLE;
    }
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTCreateDevice(IN OUT D3DKMT_CREATEDEVICE *createDevice) {
    if (createDevice == nullptr || createDevice->hAdapter != ADAPTER_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    createDevice->hDevice = DEVICE_HANDLE;
    setMockCreateDeviceParams(*createDevice);
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTDestroyDevice(IN CONST D3DKMT_DESTROYDEVICE *destoryDevice) {
    if (destoryDevice == nullptr || destoryDevice->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTCreatePagingQueue(IN OUT D3DKMT_CREATEPAGINGQUEUE *createQueue) {
    if (createQueue == nullptr || (createQueue->hDevice != DEVICE_HANDLE)) {
        return STATUS_INVALID_PARAMETER;
    }
    createQueue->hPagingQueue = PAGINGQUEUE_HANDLE;
    createQueue->hSyncObject = PAGINGQUEUE_SYNCOBJECT_HANDLE;
    createQueue->FenceValueCPUVirtualAddress = &PagingFence;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTDestroyPagingQueue(IN OUT D3DDDI_DESTROYPAGINGQUEUE *destoryQueue) {
    if (destoryQueue == nullptr || destoryQueue->hPagingQueue != PAGINGQUEUE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

static D3DKMT_CREATECONTEXTVIRTUAL createContextData{};
static CREATECONTEXT_PVTDATA createContextPrivateData{};

NTSTATUS __stdcall mockD3DKMTCreateContextVirtual(IN D3DKMT_CREATECONTEXTVIRTUAL *createContext) {
    if (createContext == nullptr || createContext->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }

    createContextData = *createContext;
    if (createContext->pPrivateDriverData) {
        createContextPrivateData = *((CREATECONTEXT_PVTDATA *)createContext->pPrivateDriverData);
        createContextData.pPrivateDriverData = &createContextPrivateData;
    }

    if ((createContext->PrivateDriverDataSize != 0 && createContext->pPrivateDriverData == nullptr) ||
        (createContext->PrivateDriverDataSize == 0 && createContext->pPrivateDriverData != nullptr)) {
        return STATUS_INVALID_PARAMETER;
    }
    createContext->hContext = CONTEXT_HANDLE;
    return STATUS_SUCCESS;
}

static bool failOnSetContextSchedulingPriority = false;
static D3DKMT_SETCONTEXTSCHEDULINGPRIORITY setContextSchedulingPriorityData{};

NTSTATUS __stdcall mockD3DKMTSetContextSchedulingPriority(_In_ CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *setContextSchedulingPriority) {
    setContextSchedulingPriorityData = *setContextSchedulingPriority;

    if (failOnSetContextSchedulingPriority) {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTDestroyContext(IN CONST D3DKMT_DESTROYCONTEXT *destroyContext) {
    if (destroyContext == nullptr || destroyContext->hContext != CONTEXT_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

static D3DKMT_CREATEALLOCATION pallocation{};

NTSTATUS __stdcall mockD3DKMTCreateAllocation(IN OUT D3DKMT_CREATEALLOCATION *allocation) {
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS __stdcall mockD3DKMTCreateAllocation2(IN OUT D3DKMT_CREATEALLOCATION *allocation) {
    D3DDDI_ALLOCATIONINFO2 *allocationInfo;
    int numOfAllocations;
    bool createResource;
    bool globalShare;
    if (allocation == nullptr || allocation->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    pallocation = *allocation;
    allocationInfo = allocation->pAllocationInfo2;
    if (allocationInfo == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    numOfAllocations = allocation->NumAllocations;
    createResource = allocation->Flags.CreateResource;
    globalShare = allocation->Flags.CreateShared;
    if (createResource) {
        allocation->hResource = RESOURCE_HANDLE;
    }
    if (globalShare) {
        allocation->hGlobalShare = RESOURCE_HANDLE;
    }

    for (int i = 0; i < numOfAllocations; ++i) {
        if (allocationInfo != NULL) {
            allocationInfo->hAllocation = ALLOCATION_HANDLE;
        }
        allocationInfo++;
    }

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTShareObjects(UINT cObjects, const D3DKMT_HANDLE *hObjects, POBJECT_ATTRIBUTES pObjectAttributes, DWORD dwDesiredAccess, HANDLE *phSharedNtHandle) {
    *phSharedNtHandle = reinterpret_cast<HANDLE>(reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(0x123)));
    return STATUS_SUCCESS;
}

static unsigned int DestroyAllocationWithResourceHandleCalled = 0u;
static D3DKMT_DESTROYALLOCATION2 destroyalloc2{};
static D3DKMT_HANDLE LastDestroyedResourceHandle = 0;
static D3DKMT_CREATEDEVICE CreateDeviceParams{};

NTSTATUS __stdcall mockD3DKMTDestroyAllocation2(IN CONST D3DKMT_DESTROYALLOCATION2 *destroyAllocation) {
    int numOfAllocations;
    const D3DKMT_HANDLE *allocationList;
    LastDestroyedResourceHandle = 0;
    if (destroyAllocation == nullptr || destroyAllocation->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    destroyalloc2 = *destroyAllocation;
    numOfAllocations = destroyAllocation->AllocationCount;
    allocationList = destroyAllocation->phAllocationList;

    for (int i = 0; i < numOfAllocations; ++i) {
        if (allocationList != NULL) {
            if (*allocationList != ALLOCATION_HANDLE) {
                return STATUS_UNSUCCESSFUL;
            }
        }
        allocationList++;
    }
    if (numOfAllocations == 0 && destroyAllocation->hResource == 0u) {
        return STATUS_UNSUCCESSFUL;
    }
    if (destroyAllocation->hResource) {
        DestroyAllocationWithResourceHandleCalled = 1;
        LastDestroyedResourceHandle = destroyAllocation->hResource;
    }

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTMapGpuVirtualAddress(IN OUT D3DDDI_MAPGPUVIRTUALADDRESS *mapGpuVA) {
    if (mapGpuVA == nullptr) {
        memset(&gLastCallMapGpuVaArg, 0, sizeof(gLastCallMapGpuVaArg));
        return STATUS_INVALID_PARAMETER;
    }

    memcpy(&gLastCallMapGpuVaArg, mapGpuVA, sizeof(gLastCallMapGpuVaArg));

    if (mapGpuVA->hPagingQueue != PAGINGQUEUE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    if (mapGpuVA->hAllocation != ALLOCATION_HANDLE && mapGpuVA->hAllocation != NT_ALLOCATION_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    if (mapGpuVA->MinimumAddress != 0) {
        if (mapGpuVA->BaseAddress != 0 && mapGpuVA->BaseAddress < mapGpuVA->MinimumAddress) {
            return STATUS_INVALID_PARAMETER;
        }
    }
    if (mapGpuVA->MaximumAddress != 0) {
        if (mapGpuVA->BaseAddress != 0 && mapGpuVA->BaseAddress > mapGpuVA->MaximumAddress) {
            return STATUS_INVALID_PARAMETER;
        }
    }
    if (mapGpuVA->BaseAddress == 0) {
        if (mapGpuVA->MinimumAddress) {
            mapGpuVA->VirtualAddress = mapGpuVA->MinimumAddress;
        } else {
            mapGpuVA->VirtualAddress = MemoryConstants::pageSize64k;
        }
    } else {
        mapGpuVA->VirtualAddress = mapGpuVA->BaseAddress;
    }

    if (gMapGpuVaFailConfigMax != 0) {
        if (gMapGpuVaFailConfigMax > gMapGpuVaFailConfigCount) {
            gMapGpuVaFailConfigCount++;
            return STATUS_UNSUCCESSFUL;
        }
    }

    mapGpuVA->PagingFenceValue = 1;
    return STATUS_PENDING;
}

NTSTATUS __stdcall mockD3DKMTReserveGpuVirtualAddress(IN OUT D3DDDI_RESERVEGPUVIRTUALADDRESS *reserveGpuVirtualAddress) {
    gLastCallReserveGpuVaArg = *reserveGpuVirtualAddress;
    reserveGpuVirtualAddress->VirtualAddress = reserveGpuVirtualAddress->MinimumAddress;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTQueryAdapterInfo(IN CONST D3DKMT_QUERYADAPTERINFO *queryAdapterInfo) {
    if (queryAdapterInfo == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    if (queryAdapterInfo->Type == KMTQAITYPE_UMDRIVERPRIVATE) {
        if (queryAdapterInfo->pPrivateDriverData == NULL) {
            return STATUS_INVALID_PARAMETER;
        }
        if (queryAdapterInfo->PrivateDriverDataSize == 0) {
            return STATUS_INVALID_PARAMETER;
        }
    }

    if (queryAdapterInfo->Type == KMTQAITYPE_ADAPTERTYPE) {
        D3DKMT_ADAPTERTYPE *adapterType = reinterpret_cast<D3DKMT_ADAPTERTYPE *>(queryAdapterInfo->pPrivateDriverData);
        if (queryAdapterInfo->hAdapter == ADAPTER_HANDLE) {
            adapterType->RenderSupported = 1;
        } else if (queryAdapterInfo->hAdapter == SHADOW_ADAPTER_HANDLE) {
            adapterType->RenderSupported = 0;
        } else {
            return STATUS_INVALID_PARAMETER;
        }

        return STATUS_SUCCESS;
    }
    if (queryAdapterInfo->Type == KMTQAITYPE_UMDRIVERPRIVATE) {
        if (queryAdapterInfo->hAdapter != ADAPTER_HANDLE && queryAdapterInfo->hAdapter != SHADOW_ADAPTER_HANDLE) {
            return STATUS_INVALID_PARAMETER;
        }
        ADAPTER_INFO_KMD *adapterInfo = reinterpret_cast<ADAPTER_INFO_KMD *>(queryAdapterInfo->pPrivateDriverData);

        adapterInfo->GfxPlatform = gAdapterInfo.GfxPlatform;
        adapterInfo->SystemInfo = gAdapterInfo.SystemInfo;
        adapterInfo->SkuTable = gAdapterInfo.SkuTable;
        adapterInfo->WaTable = gAdapterInfo.WaTable;
        adapterInfo->CacheLineSize = 64;
        adapterInfo->MinRenderFreq = 350;
        adapterInfo->MaxRenderFreq = 1150;

        adapterInfo->SizeOfDmaBuffer = 32768;
        adapterInfo->GfxMemorySize = 2181038080;
        adapterInfo->SystemSharedMemory = 4249540608;
        adapterInfo->SystemVideoMemory = 0;
        adapterInfo->GfxTimeStampFreq = 1;

        adapterInfo->GfxPartition.Standard.Base = gAdapterInfo.GfxPartition.Standard.Base;
        adapterInfo->GfxPartition.Standard.Limit = gAdapterInfo.GfxPartition.Standard.Limit;
        adapterInfo->GfxPartition.Standard64KB.Base = gAdapterInfo.GfxPartition.Standard64KB.Base;
        adapterInfo->GfxPartition.Standard64KB.Limit = gAdapterInfo.GfxPartition.Standard64KB.Limit;

        adapterInfo->GfxPartition.SVM.Base = gAdapterInfo.GfxPartition.SVM.Base;
        adapterInfo->GfxPartition.SVM.Limit = gAdapterInfo.GfxPartition.SVM.Limit;
        adapterInfo->GfxPartition.Heap32[0].Base = gAdapterInfo.GfxPartition.Heap32[0].Base;
        adapterInfo->GfxPartition.Heap32[0].Limit = gAdapterInfo.GfxPartition.Heap32[0].Limit;
        adapterInfo->GfxPartition.Heap32[1].Base = gAdapterInfo.GfxPartition.Heap32[1].Base;
        adapterInfo->GfxPartition.Heap32[1].Limit = gAdapterInfo.GfxPartition.Heap32[1].Limit;
        adapterInfo->GfxPartition.Heap32[2].Base = gAdapterInfo.GfxPartition.Heap32[2].Base;
        adapterInfo->GfxPartition.Heap32[2].Limit = gAdapterInfo.GfxPartition.Heap32[2].Limit;
        adapterInfo->GfxPartition.Heap32[3].Base = gAdapterInfo.GfxPartition.Heap32[3].Base;
        adapterInfo->GfxPartition.Heap32[3].Limit = gAdapterInfo.GfxPartition.Heap32[3].Limit;

        adapterInfo->stAdapterBDF.Data = gAdapterBDF.Data;
        return STATUS_SUCCESS;
    }

    if (KMTQAITYPE_QUERYREGISTRY == queryAdapterInfo->Type) {
        const wchar_t *driverStorePathStr = L"some/path/fffff";

        if ((nullptr == queryAdapterInfo->pPrivateDriverData) || (sizeof(D3DDDI_QUERYREGISTRY_INFO) > queryAdapterInfo->PrivateDriverDataSize)) {
            return STATUS_INVALID_PARAMETER;
        }

        D3DDDI_QUERYREGISTRY_INFO *queryRegistryInfo = reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(queryAdapterInfo->pPrivateDriverData);
        if (D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH != queryRegistryInfo->QueryType) {
            return STATUS_INVALID_PARAMETER;
        }
        if (0U != queryRegistryInfo->QueryFlags.Value) {
            return STATUS_INVALID_PARAMETER;
        }
        bool regValueNameIsEmpty = std::wstring(std::wstring(queryRegistryInfo->ValueName[0], queryRegistryInfo->ValueName[0] + sizeof(queryRegistryInfo->ValueName)).c_str()).empty();
        if (false == regValueNameIsEmpty) {
            return STATUS_INVALID_PARAMETER;
        }
        if (0U != queryRegistryInfo->ValueType) {
            return STATUS_INVALID_PARAMETER;
        }
        if (0U != queryRegistryInfo->PhysicalAdapterIndex) {
            return STATUS_INVALID_PARAMETER;
        }

        if (D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH != queryRegistryInfo->QueryType) {
            return STATUS_INVALID_PARAMETER;
        }

        queryRegistryInfo->OutputValueSize = static_cast<ULONG>(std::wstring(driverStorePathStr).size() * sizeof(wchar_t));
        if (queryAdapterInfo->PrivateDriverDataSize < queryRegistryInfo->OutputValueSize + sizeof(D3DDDI_QUERYREGISTRY_INFO)) {
            queryRegistryInfo->Status = D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW;
            return STATUS_SUCCESS;
        }

        memcpy_s(queryRegistryInfo->OutputString, queryAdapterInfo->PrivateDriverDataSize - sizeof(D3DDDI_QUERYREGISTRY_INFO),
                 driverStorePathStr, queryRegistryInfo->OutputValueSize);

        queryRegistryInfo->Status = D3DDDI_QUERYREGISTRY_STATUS_SUCCESS;
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS __stdcall mockD3DKMTMakeResident(IN OUT D3DDDI_MAKERESIDENT *makeResident) {
    if (makeResident == nullptr || makeResident->hPagingQueue != PAGINGQUEUE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    makeResident->PagingFenceValue = 0;
    return STATUS_PENDING;
}

static UINT totalPrivateSize = 0u;
static UINT gmmSize = 0u;
static void *gmmPtr = nullptr;
static UINT numberOfAllocsToReturn = 0u;

NTSTATUS __stdcall mockD3DKMTOpenResource(IN OUT D3DKMT_OPENRESOURCE *openResurce) {
    openResurce->hResource = RESOURCE_HANDLE;
    openResurce->pOpenAllocationInfo[0].hAllocation = ALLOCATION_HANDLE;
    openResurce->pOpenAllocationInfo[0].pPrivateDriverData = gmmPtr;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTOpenResourceFromNtHandle(IN OUT D3DKMT_OPENRESOURCEFROMNTHANDLE *openResurce) {
    openResurce->hResource = NT_RESOURCE_HANDLE;
    openResurce->pOpenAllocationInfo2[0].hAllocation = NT_ALLOCATION_HANDLE;
    openResurce->pOpenAllocationInfo2[0].pPrivateDriverData = gmmPtr;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTQueryResourceInfo(IN OUT D3DKMT_QUERYRESOURCEINFO *queryResourceInfo) {
    if (queryResourceInfo->hDevice != DEVICE_HANDLE || queryResourceInfo->hGlobalShare == INVALID_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    queryResourceInfo->TotalPrivateDriverDataSize = totalPrivateSize;
    queryResourceInfo->PrivateRuntimeDataSize = gmmSize;
    queryResourceInfo->NumAllocations = numberOfAllocsToReturn;

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTQueryResourceInfoFromNtHandle(IN OUT D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *queryResourceInfo) {
    if (queryResourceInfo->hDevice != DEVICE_HANDLE || queryResourceInfo->hNtHandle == reinterpret_cast<void *>(INVALID_HANDLE)) {
        return STATUS_INVALID_PARAMETER;
    }
    queryResourceInfo->TotalPrivateDriverDataSize = totalPrivateSize;
    queryResourceInfo->PrivateRuntimeDataSize = gmmSize;
    queryResourceInfo->NumAllocations = numberOfAllocsToReturn;

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTLock2(IN OUT D3DKMT_LOCK2 *lock2) {
    if (lock2->hAllocation == 0 || lock2->hDevice == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    lock2->pData = (void *)65536;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTUnlock2(IN CONST D3DKMT_UNLOCK2 *unlock2) {
    if (unlock2->hAllocation == 0 || unlock2->hDevice == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

static size_t cpuFence = 0;

static bool createSynchronizationObject2FailCall = false;

NTSTATUS __stdcall mockD3DKMTCreateSynchronizationObject2(IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *synchObject) {
    if (synchObject == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    if (createSynchronizationObject2FailCall) {
        return STATUS_INVALID_PARAMETER;
    }

    synchObject->Info.MonitoredFence.FenceValueCPUVirtualAddress = &cpuFence;
    synchObject->Info.MonitoredFence.FenceValueGPUVirtualAddress = 3;
    synchObject->hSyncObject = 4;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTSetAllocationPriority(IN CONST D3DKMT_SETALLOCATIONPRIORITY *setAllocationPriority) {
    if (setAllocationPriority == nullptr || setAllocationPriority->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }

    if (setAllocationPriority->hResource == static_cast<D3DKMT_HANDLE>(NULL) && (setAllocationPriority->AllocationCount == 0 || setAllocationPriority->phAllocationList == reinterpret_cast<void *>(NULL))) {
        return STATUS_INVALID_PARAMETER;
    }
    if (setAllocationPriority->pPriorities == reinterpret_cast<void *>(NULL)) {
        return STATUS_INVALID_PARAMETER;
    }
    auto priority = setAllocationPriority->pPriorities[0];
    gLastPriority = priority;
    for (auto i = 0u; i < setAllocationPriority->AllocationCount; i++) {
        if (setAllocationPriority->phAllocationList[i] == 0 || setAllocationPriority->pPriorities[i] != priority) {
            return STATUS_INVALID_PARAMETER;
        }
    }

    return STATUS_SUCCESS;
}

static D3DKMT_CREATEHWQUEUE createHwQueueData{};

NTSTATUS __stdcall mockD3DKMTCreateHwQueue(IN OUT D3DKMT_CREATEHWQUEUE *createHwQueue) {
    createHwQueue->hHwQueueProgressFence = 1;
    createHwQueue->HwQueueProgressFenceCPUVirtualAddress = reinterpret_cast<void *>(2);
    createHwQueue->HwQueueProgressFenceGPUVirtualAddress = 3;
    createHwQueue->hHwQueue = 4;
    createHwQueueData = *createHwQueue;
    return STATUS_SUCCESS;
}

static D3DKMT_DESTROYHWQUEUE destroyHwQueueData{};

NTSTATUS __stdcall mockD3DKMTDestroyHwQueue(IN CONST D3DKMT_DESTROYHWQUEUE *destroyHwQueue) {
    destroyHwQueueData = *destroyHwQueue;
    return STATUS_SUCCESS;
}

static D3DKMT_SUBMITCOMMANDTOHWQUEUE submitCommandToHwQueueData{};

NTSTATUS __stdcall mockD3DKMTSubmitCommandToHwQueue(IN CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *submitCommandToHwQueue) {
    submitCommandToHwQueueData = *submitCommandToHwQueue;
    return STATUS_SUCCESS;
}

static D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySynchronizationObjectData{};

NTSTATUS __stdcall mockD3DKMTDestroySynchronizationObject(IN CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *destroySynchronizationObject) {
    destroySynchronizationObjectData = *destroySynchronizationObject;
    return STATUS_SUCCESS;
}

static bool registerTrimNotificationFailCall = false;

NTSTATUS __stdcall mockD3DKMTRegisterTrimNotification(IN D3DKMT_REGISTERTRIMNOTIFICATION *registerTrimNotification) {
    if (registerTrimNotificationFailCall) {
        return STATUS_INVALID_PARAMETER;
    }
    registerTrimNotification->Handle = TRIM_CALLBACK_HANDLE;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTOpenAdapterFromHdc(IN OUT D3DKMT_OPENADAPTERFROMHDC *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTDestroyAllocation(IN CONST D3DKMT_DESTROYALLOCATION *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTCloseAdapter(IN CONST D3DKMT_CLOSEADAPTER *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTLock(IN OUT D3DKMT_LOCK *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTUnlock(IN CONST D3DKMT_UNLOCK *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTRender(IN OUT D3DKMT_RENDER *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTCreateSynchronizationObject(IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTSignalSynchronizationObject(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTWaitForSynchronizationObject(IN OUT CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTWaitForSynchronizationObjectFromCpu(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTSignalSynchronizationObjectFromCpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTWaitForSynchronizationObjectFromGpu(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTSignalSynchronizationObjectFromGpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTFreeGpuVirtualAddress(IN CONST D3DKMT_FREEGPUVIRTUALADDRESS *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTUpdateGpuVirtualAddress(IN CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTSubmitCommand(IN CONST D3DKMT_SUBMITCOMMAND *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTEvict(IN OUT D3DKMT_EVICT *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTGetDeviceState(IN OUT D3DKMT_GETDEVICESTATE *) {
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mockD3DKMTUnregisterTrimNotification(IN D3DKMT_UNREGISTERTRIMNOTIFICATION *) {
    return STATUS_SUCCESS;
}

NTSTATUS setMockSizes(void *inGmmPtr, UINT inNumAllocsToReturn, UINT inGmmSize, UINT inTotalPrivateSize) {
    gmmSize = inGmmSize;
    gmmPtr = inGmmPtr;
    totalPrivateSize = inTotalPrivateSize;
    numberOfAllocsToReturn = inNumAllocsToReturn;
    return STATUS_SUCCESS;
}

NTSTATUS getMockSizes(UINT &destroyAlloactionWithResourceHandleCalled, D3DKMT_DESTROYALLOCATION2 *&ptrDestroyAlloc) {
    destroyAlloactionWithResourceHandleCalled = DestroyAllocationWithResourceHandleCalled;
    ptrDestroyAlloc = &destroyalloc2;
    return NTSTATUS();
}

D3DKMT_HANDLE getMockLastDestroyedResHandle() {
    return LastDestroyedResourceHandle;
}

void setMockLastDestroyedResHandle(D3DKMT_HANDLE handle) {
    LastDestroyedResourceHandle = handle;
}

D3DKMT_CREATEDEVICE getMockCreateDeviceParams() {
    return CreateDeviceParams;
}

void setMockCreateDeviceParams(D3DKMT_CREATEDEVICE params) {
    CreateDeviceParams = params;
}

D3DKMT_CREATEALLOCATION *getMockAllocation() {
    return &pallocation;
}

ADAPTER_INFO *getAdapterInfoAddress() {
    return &gAdapterInfo;
}

D3DDDI_MAPGPUVIRTUALADDRESS *getLastCallMapGpuVaArg() {
    return &gLastCallMapGpuVaArg;
}

D3DDDI_RESERVEGPUVIRTUALADDRESS *getLastCallReserveGpuVaArg() {
    return &gLastCallReserveGpuVaArg;
}

void setMapGpuVaFailConfig(uint32_t count, uint32_t max) {
    gMapGpuVaFailConfigCount = count;
    gMapGpuVaFailConfigMax = max;
}

D3DKMT_CREATECONTEXTVIRTUAL *getCreateContextData() {
    return &createContextData;
}

D3DKMT_CREATEHWQUEUE *getCreateHwQueueData() {
    return &createHwQueueData;
}

D3DKMT_DESTROYHWQUEUE *getDestroyHwQueueData() {
    return &destroyHwQueueData;
}

D3DKMT_SUBMITCOMMANDTOHWQUEUE *getSubmitCommandToHwQueueData() {
    return &submitCommandToHwQueueData;
}

D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *getDestroySynchronizationObjectData() {
    return &destroySynchronizationObjectData;
}

VOID *getMonitorFenceCpuFenceAddress() {
    return &cpuFence;
}

bool *getCreateSynchronizationObject2FailCall() {
    return &createSynchronizationObject2FailCall;
}

bool *getRegisterTrimNotificationFailCall() {
    return &registerTrimNotificationFailCall;
}

uint32_t getLastPriority() {
    return gLastPriority;
}

void setAdapterBDF(ADAPTER_BDF &adapterBDF) {
    gAdapterBDF = adapterBDF;
}

bool *getFailOnSetContextSchedulingPriorityCall() {
    return &failOnSetContextSchedulingPriority;
}

D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *getSetContextSchedulingPriorityDataCall() {
    return &setContextSchedulingPriorityData;
}
