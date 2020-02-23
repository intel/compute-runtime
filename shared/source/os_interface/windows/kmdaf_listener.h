/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

namespace NEO {

struct KmDafListener {

    MOCKABLE_VIRTUAL void notifyLock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DDDICB_LOCKFLAGS *pLockFlags, PFND3DKMT_ESCAPE pfnEscape);

    MOCKABLE_VIRTUAL void notifyUnlock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape);

    MOCKABLE_VIRTUAL void notifyMapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape);

    MOCKABLE_VIRTUAL void notifyUnmapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape);

    MOCKABLE_VIRTUAL void notifyMakeResident(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape);

    MOCKABLE_VIRTUAL void notifyEvict(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape);

    MOCKABLE_VIRTUAL void notifyWriteTarget(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, PFND3DKMT_ESCAPE pfnEscape);
};
} // namespace NEO
