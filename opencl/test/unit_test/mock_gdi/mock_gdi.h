/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/windows/os_time_win.h"

#include <d3d10_1.h>
#include <d3d9types.h>

#include "Windows.h"
#include "umKmInc/sharedata.h"
#include <d3d10.h>
#include <d3dkmthk.h>

#define DECL_FUNCTIONS()                                                                                 \
    FUNCTION(OpenAdapterFromHdc, IN OUT D3DKMT_OPENADAPTERFROMHDC *)                                     \
    FUNCTION(DestroyAllocation, IN CONST D3DKMT_DESTROYALLOCATION *)                                     \
    FUNCTION(CloseAdapter, IN CONST D3DKMT_CLOSEADAPTER *)                                               \
    FUNCTION(Lock, IN OUT D3DKMT_LOCK *)                                                                 \
    FUNCTION(Unlock, IN CONST D3DKMT_UNLOCK *)                                                           \
    FUNCTION(Render, IN OUT D3DKMT_RENDER *)                                                             \
    FUNCTION(CreateSynchronizationObject, IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT *)                   \
    FUNCTION(SignalSynchronizationObject, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *)                 \
    FUNCTION(WaitForSynchronizationObject, IN OUT CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *)           \
    FUNCTION(WaitForSynchronizationObjectFromCpu, IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *) \
    FUNCTION(SignalSynchronizationObjectFromCpu, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *)   \
    FUNCTION(WaitForSynchronizationObjectFromGpu, IN CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *) \
    FUNCTION(SignalSynchronizationObjectFromGpu, IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *)   \
    FUNCTION(FreeGpuVirtualAddress, IN CONST D3DKMT_FREEGPUVIRTUALADDRESS *)                             \
    FUNCTION(UpdateGpuVirtualAddress, IN CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *)                         \
    FUNCTION(SubmitCommand, IN CONST D3DKMT_SUBMITCOMMAND *)                                             \
    FUNCTION(Evict, IN OUT D3DKMT_EVICT *)                                                               \
    FUNCTION(GetDeviceState, IN OUT D3DKMT_GETDEVICESTATE *)                                             \
    FUNCTION(UnregisterTrimNotification, IN D3DKMT_UNREGISTERTRIMNOTIFICATION *)

#define STR(X) #X

#define FUNCTION(FUNC_NAME, FUNC_ARG)                \
    NTSTATUS __stdcall D3DKMT##FUNC_NAME(FUNC_ARG) { \
        return STATUS_SUCCESS;                       \
    }

#define ADAPTER_HANDLE (static_cast<D3DKMT_HANDLE>(0x40001234))
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

NTSTATUS MockSetSizes(void *gmmPtr, UINT numAllocsToReturn, UINT gmmSize, UINT totalPrivateSize);
NTSTATUS GetMockSizes(UINT &destroyAlloactionWithResourceHandleCalled, D3DKMT_DESTROYALLOCATION2 *&ptrDestroyAlloc);
D3DKMT_HANDLE GetMockLastDestroyedResHandle();
void SetMockLastDestroyedResHandle(D3DKMT_HANDLE handle);
D3DKMT_CREATEDEVICE GetMockCreateDeviceParams();
void SetMockCreateDeviceParams(D3DKMT_CREATEDEVICE params);
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
void InitGfxPartition();
VOID *getMonitorFenceCpuFenceAddress();
bool *getCreateSynchronizationObject2FailCall();
bool *getRegisterTrimNotificationFailCall();
