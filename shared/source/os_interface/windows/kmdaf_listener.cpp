/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/kmdaf_listener.h"
#pragma warning(push)           // save the current state
#pragma warning(disable : 4189) // disable warning 4189 (unused local variable)
#include "kmdaf.h"
#pragma warning(pop) // restore state.

namespace NEO {

void KmDafListener::notifyLock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DDDICB_LOCKFLAGS *pLockFlags, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_LOCK(ftrKmdDaf, hAdapter, hDevice, hAllocation, pLockFlags, pfnEscape);
}

void KmDafListener::notifyUnlock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_UNLOCK(ftrKmdDaf, hAdapter, hDevice, phAllocation, allocations, pfnEscape);
}

void KmDafListener::notifyMapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DGPU_VIRTUAL_ADDRESS gpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_MAP_GPUVA(ftrKmdDaf, hAdapter, hDevice, hAllocation, gpuVirtualAddress, pfnEscape);
}

void KmDafListener::notifyUnmapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, D3DGPU_VIRTUAL_ADDRESS gpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) {
    KM_DAF_NOTIFY_UNMAP_GPUVA(ftrKmdDaf, hAdapter, hDevice, gpuVirtualAddress, pfnEscape);
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
} // namespace NEO
