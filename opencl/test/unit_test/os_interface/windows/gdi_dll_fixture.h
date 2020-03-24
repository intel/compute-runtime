/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/mock_gdi/mock_gdi.h"

using namespace NEO;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace);

struct GdiDllFixture {
    virtual void SetUp() {
        const HardwareInfo *hwInfo = defaultHwInfo.get();
        mockGdiDll.reset(setAdapterInfo(&hwInfo->platform, &hwInfo->gtSystemInfo, hwInfo->capabilityTable.gpuAddressSpace));

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
        getLastCallReserveGpuVaArgFcn = reinterpret_cast<decltype(&getLastCallReserveGpuVaArg)>(mockGdiDll->getProcAddress("getLastCallReserveGpuVaArg"));
        setMapGpuVaFailConfigFcn = reinterpret_cast<decltype(&setMapGpuVaFailConfig)>(mockGdiDll->getProcAddress("setMapGpuVaFailConfig"));
        setMapGpuVaFailConfigFcn(0, 0);
        getCreateContextDataFcn = reinterpret_cast<decltype(&getCreateContextData)>(mockGdiDll->getProcAddress("getCreateContextData"));
        getCreateHwQueueDataFcn = reinterpret_cast<decltype(&getCreateHwQueueData)>(mockGdiDll->getProcAddress("getCreateHwQueueData"));
        getDestroyHwQueueDataFcn = reinterpret_cast<decltype(&getDestroyHwQueueData)>(mockGdiDll->getProcAddress("getDestroyHwQueueData"));
        getSubmitCommandToHwQueueDataFcn =
            reinterpret_cast<decltype(&getSubmitCommandToHwQueueData)>(mockGdiDll->getProcAddress("getSubmitCommandToHwQueueData"));
        getDestroySynchronizationObjectDataFcn =
            reinterpret_cast<decltype(&getDestroySynchronizationObjectData)>(mockGdiDll->getProcAddress("getDestroySynchronizationObjectData"));
        getMonitorFenceCpuFenceAddressFcn =
            reinterpret_cast<decltype(&getMonitorFenceCpuFenceAddress)>(mockGdiDll->getProcAddress("getMonitorFenceCpuFenceAddress"));
        getCreateSynchronizationObject2FailCallFcn =
            reinterpret_cast<decltype(&getCreateSynchronizationObject2FailCall)>(mockGdiDll->getProcAddress("getCreateSynchronizationObject2FailCall"));
        getRegisterTrimNotificationFailCallFcn =
            reinterpret_cast<decltype(&getRegisterTrimNotificationFailCall)>(mockGdiDll->getProcAddress("getRegisterTrimNotificationFailCall"));
        setMockLastDestroyedResHandleFcn((D3DKMT_HANDLE)0);
        *getDestroySynchronizationObjectDataFcn() = {};
        *getCreateSynchronizationObject2FailCallFcn() = false;
        *getRegisterTrimNotificationFailCallFcn() = false;
    }

    virtual void TearDown() {
        *getCreateHwQueueDataFcn() = {};
        *getDestroyHwQueueDataFcn() = {};
        *getSubmitCommandToHwQueueDataFcn() = {};
        *getDestroySynchronizationObjectDataFcn() = {};
        setMapGpuVaFailConfigFcn(0, 0);
        *getCreateSynchronizationObject2FailCallFcn() = false;
        *getRegisterTrimNotificationFailCallFcn() = false;
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
    decltype(&getLastCallReserveGpuVaArg) getLastCallReserveGpuVaArgFcn = nullptr;
    decltype(&setMapGpuVaFailConfig) setMapGpuVaFailConfigFcn = nullptr;
    decltype(&getCreateContextData) getCreateContextDataFcn = nullptr;
    decltype(&getCreateHwQueueData) getCreateHwQueueDataFcn = nullptr;
    decltype(&getDestroyHwQueueData) getDestroyHwQueueDataFcn = nullptr;
    decltype(&getSubmitCommandToHwQueueData) getSubmitCommandToHwQueueDataFcn = nullptr;
    decltype(&getDestroySynchronizationObjectData) getDestroySynchronizationObjectDataFcn = nullptr;
    decltype(&getMonitorFenceCpuFenceAddress) getMonitorFenceCpuFenceAddressFcn = nullptr;
    decltype(&getCreateSynchronizationObject2FailCall) getCreateSynchronizationObject2FailCallFcn = nullptr;
    decltype(&getRegisterTrimNotificationFailCall) getRegisterTrimNotificationFailCallFcn = nullptr;
};
