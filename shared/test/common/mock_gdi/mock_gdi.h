/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#define ADAPTER_HANDLE (static_cast<D3DKMT_HANDLE>(0x40001234))
#define SHADOW_ADAPTER_HANDLE (static_cast<D3DKMT_HANDLE>(0x30001234))
#define DEVICE_HANDLE (static_cast<D3DKMT_HANDLE>(0x40004321))
#define PAGINGQUEUE_HANDLE (static_cast<D3DKMT_HANDLE>(0x40005678))
#define PAGINGQUEUE_SYNCOBJECT_HANDLE (static_cast<D3DKMT_HANDLE>(0x40008765))
#define CONTEXT_HANDLE (static_cast<D3DKMT_HANDLE>(0x40001111))
#define INVALID_HANDLE (static_cast<D3DKMT_HANDLE>(0))

#define RESOURCE_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000000))
#define ALLOCATION_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000008))

#define NT_RESOURCE_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000001))
#define NT_ALLOCATION_HANDLE (static_cast<D3DKMT_HANDLE>(0x80000009))
#define TRIM_CALLBACK_HANDLE (reinterpret_cast<VOID *>(0x80123000010))

#define GPUVA (static_cast<D3DGPU_VIRTUAL_ADDRESS>(0x80123000000))

typedef struct _D3DKMT_CREATENATIVEFENCE D3DKMT_CREATENATIVEFENCE;

void mockSetAdapterInfo(const void *pGfxPlatform, const void *pGTSystemInfo, uint64_t gpuAddressSpace);
NTSTATUS __stdcall mockD3DKMTCreateAllocation(IN OUT D3DKMT_CREATEALLOCATION *allocation);
NTSTATUS __stdcall mockD3DKMTCreateAllocation2(IN OUT D3DKMT_CREATEALLOCATION *allocation);
NTSTATUS __stdcall mockD3DKMTOpenAdapterFromLuid(IN OUT CONST D3DKMT_OPENADAPTERFROMLUID *openAdapter);
NTSTATUS __stdcall mockD3DKMTShareObjects(UINT cObjects, const D3DKMT_HANDLE *hObjects, POBJECT_ATTRIBUTES pObjectAttributes, DWORD dwDesiredAccess, HANDLE *phSharedNtHandle);
NTSTATUS __stdcall mockD3DKMTDestroyAllocation2(IN CONST D3DKMT_DESTROYALLOCATION2 *destroyAllocation);
NTSTATUS __stdcall mockD3DKMTQueryAdapterInfo(IN CONST D3DKMT_QUERYADAPTERINFO *queryAdapterInfo);
NTSTATUS __stdcall mockD3DKMTCreateDevice(IN OUT D3DKMT_CREATEDEVICE *createDevice);
NTSTATUS __stdcall mockD3DKMTDestroyDevice(IN CONST D3DKMT_DESTROYDEVICE *destoryDevice);
NTSTATUS __stdcall mockD3DKMTEscape(IN CONST D3DKMT_ESCAPE *pData);
NTSTATUS __stdcall mockD3DKMTCreateContextVirtual(IN D3DKMT_CREATECONTEXTVIRTUAL *createContext);
NTSTATUS __stdcall mockD3DKMTDestroyContext(IN CONST D3DKMT_DESTROYCONTEXT *destroyContext);
NTSTATUS __stdcall mockD3DKMTOpenResource(IN OUT D3DKMT_OPENRESOURCE *openResurce);
NTSTATUS __stdcall mockD3DKMTOpenResourceFromNtHandle(IN OUT D3DKMT_OPENRESOURCEFROMNTHANDLE *openResurce);
NTSTATUS __stdcall mockD3DKMTQueryResourceInfo(IN OUT D3DKMT_QUERYRESOURCEINFO *queryResourceInfo);
NTSTATUS __stdcall mockD3DKMTQueryResourceInfoFromNtHandle(IN OUT D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *queryResourceInfo);
NTSTATUS __stdcall mockD3DKMTCreateSynchronizationObject2(IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *synchObject);
NTSTATUS __stdcall mockD3DKMTDestroySynchronizationObject(IN CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *destroySynchronizationObject);
NTSTATUS __stdcall mockD3DKMTCreateNativeFence(IN OUT D3DKMT_CREATENATIVEFENCE *synchObject);
NTSTATUS __stdcall mockD3DKMTCreatePagingQueue(IN OUT D3DKMT_CREATEPAGINGQUEUE *createQueue);
NTSTATUS __stdcall mockD3DKMTDestroyPagingQueue(IN OUT D3DDDI_DESTROYPAGINGQUEUE *destoryQueue);
NTSTATUS __stdcall mockD3DKMTLock2(IN OUT D3DKMT_LOCK2 *lock2);
NTSTATUS __stdcall mockD3DKMTUnlock2(IN CONST D3DKMT_UNLOCK2 *unlock2);
NTSTATUS __stdcall mockD3DKMTMapGpuVirtualAddress(IN OUT D3DDDI_MAPGPUVIRTUALADDRESS *mapGpuVA);
NTSTATUS __stdcall mockD3DKMTReserveGpuVirtualAddress(IN OUT D3DDDI_RESERVEGPUVIRTUALADDRESS *reserveGpuVirtualAddress);
NTSTATUS __stdcall mockD3DKMTSubmitCommandToHwQueue(IN CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *submitCommandToHwQueue);
NTSTATUS __stdcall mockD3DKMTMakeResident(IN OUT D3DDDI_MAKERESIDENT *makeResident);
NTSTATUS __stdcall mockD3DKMTRegisterTrimNotification(IN D3DKMT_REGISTERTRIMNOTIFICATION *registerTrimNotification);
NTSTATUS __stdcall mockD3DKMTSetAllocationPriority(IN CONST D3DKMT_SETALLOCATIONPRIORITY *setAllocationPriority);
NTSTATUS __stdcall mockD3DKMTSetContextSchedulingPriority(_In_ CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *setContextSchedulingPriority);
NTSTATUS __stdcall mockD3DKMTCreateHwQueue(IN OUT D3DKMT_CREATEHWQUEUE *createHwQueue);
NTSTATUS __stdcall mockD3DKMTDestroyHwQueue(IN CONST D3DKMT_DESTROYHWQUEUE *destroyHwQueue);
NTSTATUS __stdcall mockD3DKMTOpenAdapterFromHdc(IN OUT D3DKMT_OPENADAPTERFROMHDC *);
NTSTATUS __stdcall mockD3DKMTDestroyAllocation(IN CONST D3DKMT_DESTROYALLOCATION *);
NTSTATUS __stdcall mockD3DKMTCloseAdapter(IN CONST D3DKMT_CLOSEADAPTER *);
NTSTATUS __stdcall mockD3DKMTLock(IN OUT D3DKMT_LOCK *);
NTSTATUS __stdcall mockD3DKMTUnlock(IN CONST D3DKMT_UNLOCK *);
NTSTATUS __stdcall mockD3DKMTRender(IN OUT D3DKMT_RENDER *);
NTSTATUS __stdcall mockD3DKMTCreateSynchronizationObject(IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT *);
NTSTATUS __stdcall mockD3DKMTSignalSynchronizationObject(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *);
NTSTATUS __stdcall mockD3DKMTWaitForSynchronizationObject(IN OUT CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *);
NTSTATUS __stdcall mockD3DKMTWaitForSynchronizationObjectFromCpu(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *);
NTSTATUS __stdcall mockD3DKMTSignalSynchronizationObjectFromCpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *);
NTSTATUS __stdcall mockD3DKMTWaitForSynchronizationObjectFromGpu(IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *);
NTSTATUS __stdcall mockD3DKMTSignalSynchronizationObjectFromGpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *);
NTSTATUS __stdcall mockD3DKMTOpenSyncObjectFromNtHandle2(IN OUT D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *);
NTSTATUS __stdcall mockD3DKMTOpenSyncObjectNtHandleFromName(IN OUT D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *);
NTSTATUS __stdcall mockD3DKMTFreeGpuVirtualAddress(IN CONST D3DKMT_FREEGPUVIRTUALADDRESS *);
NTSTATUS __stdcall mockD3DKMTUpdateGpuVirtualAddress(IN CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *);
NTSTATUS __stdcall mockD3DKMTSubmitCommand(IN CONST D3DKMT_SUBMITCOMMAND *);
NTSTATUS __stdcall mockD3DKMTEvict(IN OUT D3DKMT_EVICT *);
NTSTATUS __stdcall mockD3DKMTGetDeviceState(IN OUT D3DKMT_GETDEVICESTATE *);
NTSTATUS __stdcall mockD3DKMTUnregisterTrimNotification(IN D3DKMT_UNREGISTERTRIMNOTIFICATION *);

NTSTATUS setMockSizes(void **gmmPtrArray, UINT numAllocsToReturn, UINT gmmSize, UINT totalPrivateSize);
NTSTATUS getMockSizes(UINT &destroyAlloactionWithResourceHandleCalled, D3DKMT_DESTROYALLOCATION2 *&ptrDestroyAlloc);
D3DKMT_HANDLE getMockLastDestroyedResHandle();
void setMockLastDestroyedResHandle(D3DKMT_HANDLE handle);
D3DKMT_CREATEDEVICE getMockCreateDeviceParams();
void setMockCreateDeviceParams(D3DKMT_CREATEDEVICE params);
D3DKMT_CREATEALLOCATION *getMockAllocation();
ADAPTER_INFO *getAdapterInfoAddress();
D3DDDI_MAPGPUVIRTUALADDRESS *getLastCallMapGpuVaArg();
D3DDDI_RESERVEGPUVIRTUALADDRESS *getLastCallReserveGpuVaArg();
void setMapGpuVaFailConfig(uint32_t count, uint32_t max);
D3DKMT_CREATECONTEXTVIRTUAL *getCreateContextData();
D3DKMT_CREATEHWQUEUE *getCreateHwQueueData();
D3DKMT_DESTROYHWQUEUE *getDestroyHwQueueData();
D3DKMT_SUBMITCOMMANDTOHWQUEUE *getSubmitCommandToHwQueueData();
D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *getDestroySynchronizationObjectData();
VOID *getMonitorFenceCpuFenceAddress();
bool *getMonitorFenceCpuAddressSelector();
bool *getCreateSynchronizationObject2FailCall();
bool *getFailOnSetContextSchedulingPriorityCall();
D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *getSetContextSchedulingPriorityDataCall();
bool *getRegisterTrimNotificationFailCall();
uint32_t getLastPriority();
void setAdapterBDF(ADAPTER_BDF &adapterBDF);
void setMockDeviceExecutionState(D3DKMT_DEVICEEXECUTION_STATE newState);
void setMockGetDeviceStateReturnValue(NTSTATUS newReturnValue, bool execution);
void initGfxPartition();
void setCapturingCreateAllocationFlags();
void getCapturedCreateAllocationFlags(D3DKMT_CREATEALLOCATIONFLAGS &capturedCreateAllocationFlags, uint32_t &numCalled);
void setSupportCreateAllocationWithReadWriteExisitingSysMemory(bool supportValue, bool &previousValue);
extern bool failCreateDevice;
extern bool failCreatePagingQueue;
