/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/kmdaf_listener.h"

namespace NEO {

void KmDafListener::notifyLock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DDDICB_LOCKFLAGS *pLockFlags, PFND3DKMT_ESCAPE pfnEscape) {
}

void KmDafListener::notifyUnlock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) {
}

void KmDafListener::notifyMapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) {
}

void KmDafListener::notifyUnmapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) {
}

void KmDafListener::notifyMakeResident(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) {
}

void KmDafListener::notifyEvict(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) {
}

void KmDafListener::notifyWriteTarget(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, PFND3DKMT_ESCAPE pfnEscape) {
}
} // namespace NEO
