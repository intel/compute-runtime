/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once
#include "runtime/os_interface/windows/kmdaf_listener.h"

namespace OCLRT {

struct KmDafListenerMock : public KmDafListener {

    inline void notifyLock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DDDICB_LOCKFLAGS *pLockFlags, PFND3DKMT_ESCAPE pfnEscape) override {
        notifyLockParametrization.ftrKmdDaf = ftrKmdDaf;
        notifyLockParametrization.hAdapter = hAdapter;
        notifyLockParametrization.hDevice = hDevice;
        notifyLockParametrization.hAllocation = hAllocation;
        notifyLockParametrization.pLockFlags = pLockFlags;
        notifyLockParametrization.pfnEscape = pfnEscape;
    }

    inline void notifyUnlock(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) override {
        notifyUnlockParametrization.ftrKmdDaf = ftrKmdDaf;
        notifyUnlockParametrization.hAdapter = hAdapter;
        notifyUnlockParametrization.hDevice = hDevice;
        notifyUnlockParametrization.phAllocation = phAllocation;
        notifyUnlockParametrization.allocations = allocations;
        notifyUnlockParametrization.pfnEscape = pfnEscape;
    }

    inline void notifyMapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) override {
        notifyMapGpuVAParametrization.ftrKmdDaf = ftrKmdDaf;
        notifyMapGpuVAParametrization.hAdapter = hAdapter;
        notifyMapGpuVAParametrization.hDevice = hDevice;
        notifyMapGpuVAParametrization.hAllocation = hAllocation;
        notifyMapGpuVAParametrization.GpuVirtualAddress = GpuVirtualAddress;
        notifyMapGpuVAParametrization.pfnEscape = pfnEscape;
    }

    inline void notifyUnmapGpuVA(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress, PFND3DKMT_ESCAPE pfnEscape) override {
        notifyUnmapGpuVAParametrization.ftrKmdDaf = ftrKmdDaf;
        notifyUnmapGpuVAParametrization.hAdapter = hAdapter;
        notifyUnmapGpuVAParametrization.hDevice = hDevice;
        notifyUnmapGpuVAParametrization.GpuVirtualAddress = GpuVirtualAddress;
        notifyUnmapGpuVAParametrization.pfnEscape = pfnEscape;
    }

    inline void notifyMakeResident(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) override {
        notifyMakeResidentParametrization.ftrKmdDaf = ftrKmdDaf;
        notifyMakeResidentParametrization.hAdapter = hAdapter;
        notifyMakeResidentParametrization.hDevice = hDevice;
        notifyMakeResidentParametrization.phAllocation = phAllocation;
        notifyMakeResidentParametrization.allocations = allocations;
        notifyMakeResidentParametrization.pfnEscape = pfnEscape;
    }

    inline void notifyEvict(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE *phAllocation, ULONG allocations, PFND3DKMT_ESCAPE pfnEscape) override {
        notifyEvictParametrization.ftrKmdDaf = ftrKmdDaf;
        notifyEvictParametrization.hAdapter = hAdapter;
        notifyEvictParametrization.hDevice = hDevice;
        notifyEvictParametrization.phAllocation = phAllocation;
        notifyEvictParametrization.allocations = allocations;
        notifyEvictParametrization.pfnEscape = pfnEscape;
    }

    inline void notifyWriteTarget(bool ftrKmdDaf, D3DKMT_HANDLE hAdapter, D3DKMT_HANDLE hDevice, const D3DKMT_HANDLE hAllocation, PFND3DKMT_ESCAPE pfnEscape) override {
        notifyWriteTargetParametrization.ftrKmdDaf = ftrKmdDaf;
        notifyWriteTargetParametrization.hAdapter = hAdapter;
        notifyWriteTargetParametrization.hDevice = hDevice;
        notifyWriteTargetParametrization.hAllocation = hAllocation;
        notifyWriteTargetParametrization.pfnEscape = pfnEscape;
    }

    struct NotifyLockParametrization {
        bool ftrKmdDaf = false;
        D3DKMT_HANDLE hAdapter = 0;
        D3DKMT_HANDLE hDevice = 0;
        D3DKMT_HANDLE hAllocation = 0;
        D3DDDICB_LOCKFLAGS *pLockFlags = nullptr;
        PFND3DKMT_ESCAPE pfnEscape = nullptr;
    } notifyLockParametrization;

    struct NotifyUnlockParametrization {
        bool ftrKmdDaf = 0;
        D3DKMT_HANDLE hAdapter = 0;
        D3DKMT_HANDLE hDevice = 0;
        const D3DKMT_HANDLE *phAllocation = nullptr;
        ULONG allocations = 0;
        PFND3DKMT_ESCAPE pfnEscape = nullptr;
    } notifyUnlockParametrization;

    struct NotifyMapGpuVAParametrization {
        bool ftrKmdDaf = false;
        D3DKMT_HANDLE hAdapter = 0;
        D3DKMT_HANDLE hDevice = 0;
        D3DKMT_HANDLE hAllocation = 0;
        D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress = 0;
        PFND3DKMT_ESCAPE pfnEscape = nullptr;
    } notifyMapGpuVAParametrization;

    struct NotifyUnmapGpuVAParametrization {
        bool ftrKmdDaf = false;
        D3DKMT_HANDLE hAdapter = 0;
        D3DKMT_HANDLE hDevice = 0;
        D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress = 0;
        PFND3DKMT_ESCAPE pfnEscape = nullptr;
    } notifyUnmapGpuVAParametrization;

    struct NotifyMakeResidentParametrization {
        bool ftrKmdDaf = 0;
        D3DKMT_HANDLE hAdapter = 0;
        D3DKMT_HANDLE hDevice = 0;
        const D3DKMT_HANDLE *phAllocation = nullptr;
        ULONG allocations = 0;
        PFND3DKMT_ESCAPE pfnEscape = nullptr;
    } notifyMakeResidentParametrization;

    struct NotifyEvictParametrization {
        bool ftrKmdDaf = 0;
        D3DKMT_HANDLE hAdapter = 0;
        D3DKMT_HANDLE hDevice = 0;
        const D3DKMT_HANDLE *phAllocation = nullptr;
        ULONG allocations = 0;
        PFND3DKMT_ESCAPE pfnEscape = nullptr;
    } notifyEvictParametrization;

    struct NotifyWriteTargetParametrization {
        bool ftrKmdDaf = 0;
        D3DKMT_HANDLE hAdapter = 0;
        D3DKMT_HANDLE hDevice = 0;
        D3DKMT_HANDLE hAllocation = 0;
        PFND3DKMT_ESCAPE pfnEscape = nullptr;
    } notifyWriteTargetParametrization;
};
} // namespace OCLRT
