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
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/os_library.h"
#include "unit_tests/mock_gdi/mock_gdi.h"

using namespace OCLRT;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo);

struct GdiDllFixture {
    virtual void SetUp() {
        const HardwareInfo hwInfo = *platformDevices[0];
        mockGdiDll.reset(setAdapterInfo(hwInfo.pPlatform, hwInfo.pSysInfo));

        setSizesFcn = reinterpret_cast<decltype(&MockSetSizes)>(mockGdiDll->getProcAddress("MockSetSizes"));
        getSizesFcn = reinterpret_cast<decltype(&GetMockSizes)>(mockGdiDll->getProcAddress("GetMockSizes"));
        getMockLastDestroyedResHandleFcn =
            reinterpret_cast<decltype(&GetMockLastDestroyedResHandle)>(mockGdiDll->getProcAddress("GetMockLastDestroyedResHandle"));
        setMockLastDestroyedResHandleFcn =
            reinterpret_cast<decltype(&SetMockLastDestroyedResHandle)>(mockGdiDll->getProcAddress("SetMockLastDestroyedResHandle"));
        getMockCreateDeviceParamsFcn =
            reinterpret_cast<decltype(&GetMockCreateDeviceParams)>(mockGdiDll->getProcAddress("GetMockCreateDeviceParams"));
        setMockCreateDeviceParamsFcn =
            reinterpret_cast<decltype(&SetMockCreateDeviceParams)>(mockGdiDll->getProcAddress("SetMockCreateDeviceParams"));
        getMockAllocationFcn = reinterpret_cast<decltype(&getMockAllocation)>(mockGdiDll->getProcAddress("getMockAllocation"));
        getAdapterInfoAddressFcn = reinterpret_cast<decltype(&getAdapterInfoAddress)>(mockGdiDll->getProcAddress("getAdapterInfoAddress"));
        getLastCallMapGpuVaArgFcn = reinterpret_cast<decltype(&getLastCallMapGpuVaArg)>(mockGdiDll->getProcAddress("getLastCallMapGpuVaArg"));
        setMapGpuVaFailConfigFcn = reinterpret_cast<decltype(&setMapGpuVaFailConfig)>(mockGdiDll->getProcAddress("setMapGpuVaFailConfig"));
        setMapGpuVaFailConfigFcn(0, 0);
        getCreateContextDataFcn = reinterpret_cast<decltype(&getCreateContextData)>(mockGdiDll->getProcAddress("getCreateContextData"));
        getCreateHwQueueDataFcn = reinterpret_cast<decltype(&getCreateHwQueueData)>(mockGdiDll->getProcAddress("getCreateHwQueueData"));
        getDestroyHwQueueDataFcn = reinterpret_cast<decltype(&getDestroyHwQueueData)>(mockGdiDll->getProcAddress("getDestroyHwQueueData"));
        getSubmitCommandToHwQueueDataFcn =
            reinterpret_cast<decltype(&getSubmitCommandToHwQueueData)>(mockGdiDll->getProcAddress("getSubmitCommandToHwQueueData"));
        setMockLastDestroyedResHandleFcn((D3DKMT_HANDLE)0);
    }

    virtual void TearDown() {
        *getCreateHwQueueDataFcn() = {};
        *getDestroyHwQueueDataFcn() = {};
        *getSubmitCommandToHwQueueDataFcn() = {};
        setMapGpuVaFailConfigFcn(0, 0);
    }

    std::unique_ptr<OsLibrary> mockGdiDll;

    decltype(&MockSetSizes) setSizesFcn = nullptr;
    decltype(&GetMockSizes) getSizesFcn = nullptr;
    decltype(&GetMockLastDestroyedResHandle) getMockLastDestroyedResHandleFcn = nullptr;
    decltype(&SetMockLastDestroyedResHandle) setMockLastDestroyedResHandleFcn = nullptr;
    decltype(&GetMockCreateDeviceParams) getMockCreateDeviceParamsFcn = nullptr;
    decltype(&SetMockCreateDeviceParams) setMockCreateDeviceParamsFcn = nullptr;
    decltype(&getMockAllocation) getMockAllocationFcn = nullptr;
    decltype(&getAdapterInfoAddress) getAdapterInfoAddressFcn = nullptr;
    decltype(&getLastCallMapGpuVaArg) getLastCallMapGpuVaArgFcn = nullptr;
    decltype(&setMapGpuVaFailConfig) setMapGpuVaFailConfigFcn = nullptr;
    decltype(&getCreateContextData) getCreateContextDataFcn = nullptr;
    decltype(&getCreateHwQueueData) getCreateHwQueueDataFcn = nullptr;
    decltype(&getDestroyHwQueueData) getDestroyHwQueueDataFcn = nullptr;
    decltype(&getSubmitCommandToHwQueueData) getSubmitCommandToHwQueueDataFcn = nullptr;
};
