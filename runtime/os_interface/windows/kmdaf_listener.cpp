/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/kmdaf_listener.h"
#include "kmdaf.h"

namespace OCLRT {

void KmDafListener::notifyLock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DDDICB_LOCKFLAGS *pLockFlags, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_LOCK(ftrKmdDaf, hAdapter, hDevice, hAllocation, pLockFlags, pfnEscape);
}

void KmDafListener::notifyUnlock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_UNLOCK(ftrKmdDaf, hAdapter, hDevice, phAllocation, allocations, pfnEscape);
}

void KmDafListener::notifyMapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_MAP_GPUVA(ftrKmdDaf, hAdapter, hDevice, hAllocation, GpuVirtualAddress, pfnEscape);
}

void KmDafListener::notifyUnmapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_UNMAP_GPUVA(ftrKmdDaf, hAdapter, hDevice, GpuVirtualAddress, pfnEscape);
}

void KmDafListener::notifyMakeResident(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_MAKERESIDENT(ftrKmdDaf, hAdapter, hDevice, phAllocation, allocations, pfnEscape);
}

void KmDafListener::notifyEvict(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_EVICT(ftrKmdDaf, hAdapter, hDevice, phAllocation, allocations, pfnEscape);
}

void KmDafListener::notifyWriteTarget(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_WRITE_TARGET(ftrKmdDaf, hAdapter, hDevice, hAllocation, pfnEscape);
}
} // namespace OCLRT
