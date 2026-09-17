/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"

using namespace NEO;

void setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace);

struct GdiDllFixture {
    void setUp() {
        const HardwareInfo *hwInfo = defaultHwInfo.get();
        setAdapterInfo(&hwInfo->platform, &hwInfo->gtSystemInfo, hwInfo->capabilityTable.gpuAddressSpace);

        setSizesFcn = &setMockSizes;
        getSizesFcn = &getMockSizes;
        getMockLastDestroyedResHandleFcn = &getMockLastDestroyedResHandle;
        setMockLastDestroyedResHandleFcn = &setMockLastDestroyedResHandle;
        getMockCreateDeviceParamsFcn = &getMockCreateDeviceParams;
        setMockCreateDeviceParamsFcn = &setMockCreateDeviceParams;
        getMockAllocationFcn = &getMockAllocation;
        getAdapterInfoAddressFcn = &getAdapterInfoAddress;
        getLastCallMapGpuVaArgFcn = &getLastCallMapGpuVaArg;
        getLastCallReserveGpuVaArgFcn = &getLastCallReserveGpuVaArg;
        setMapGpuVaFailConfigFcn = &setMapGpuVaFailConfig;
        setMapGpuVaFailConfigFcn(0, 0);
        getCreateContextDataFcn = &getCreateContextData;
        getCreateHwQueueDataFcn = &getCreateHwQueueData;
        getDestroyHwQueueDataFcn = &getDestroyHwQueueData;
        getSubmitCommandToHwQueueDataFcn = &getSubmitCommandToHwQueueData;
        getDestroySynchronizationObjectDataFcn = &getDestroySynchronizationObjectData;
        getMonitorFenceCpuFenceAddressFcn = &getMonitorFenceCpuFenceAddress;
        getMonitorFenceCpuAddressSelectorFcn = &getMonitorFenceCpuAddressSelector;
        getCreateSynchronizationObject2FailCallFcn = &getCreateSynchronizationObject2FailCall;
        getFailOnSetContextSchedulingPriorityCallFcn = &getFailOnSetContextSchedulingPriorityCall;
        getSetContextSchedulingPriorityDataCallFcn = &getSetContextSchedulingPriorityDataCall;
        getRegisterTrimNotificationFailCallFcn = &getRegisterTrimNotificationFailCall;
        getLastPriorityFcn = &getLastPriority;
        setAdapterBDFFcn = &setAdapterBDF;
        setMockDeviceExecutionStateFcn = &setMockDeviceExecutionState;
        setMockGetDeviceStateReturnValueFcn = &setMockGetDeviceStateReturnValue;
        getCapturedCreateAllocationFlagsFcn = &getCapturedCreateAllocationFlags;
        setCapturingCreateAllocationFlagsFcn = &setCapturingCreateAllocationFlags;
        setSupportCreateAllocationWithReadWriteExisitingSysMemoryFcn = &setSupportCreateAllocationWithReadWriteExisitingSysMemory;
        setMockLastDestroyedResHandleFcn((D3DKMT_HANDLE)0);
        *getDestroySynchronizationObjectDataFcn() = {};
        *getCreateSynchronizationObject2FailCallFcn() = false;
        *getFailOnSetContextSchedulingPriorityCallFcn() = false;
        *getSetContextSchedulingPriorityDataCallFcn() = {};
        *getRegisterTrimNotificationFailCallFcn() = false;
    }

    void tearDown() {
        *getCreateHwQueueDataFcn() = {};
        *getDestroyHwQueueDataFcn() = {};
        *getSubmitCommandToHwQueueDataFcn() = {};
        *getDestroySynchronizationObjectDataFcn() = {};
        setMapGpuVaFailConfigFcn(0, 0);
        *getCreateSynchronizationObject2FailCallFcn() = false;
        *getFailOnSetContextSchedulingPriorityCallFcn() = false;
        *getSetContextSchedulingPriorityDataCallFcn() = {};
        *getRegisterTrimNotificationFailCallFcn() = false;
    }

    decltype(&setMockSizes) setSizesFcn = nullptr;
    decltype(&getMockSizes) getSizesFcn = nullptr;
    decltype(&getMockLastDestroyedResHandle) getMockLastDestroyedResHandleFcn = nullptr;
    decltype(&setMockLastDestroyedResHandle) setMockLastDestroyedResHandleFcn = nullptr;
    decltype(&getMockCreateDeviceParams) getMockCreateDeviceParamsFcn = nullptr;
    decltype(&setMockCreateDeviceParams) setMockCreateDeviceParamsFcn = nullptr;
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
    decltype(&getMonitorFenceCpuAddressSelector) getMonitorFenceCpuAddressSelectorFcn = nullptr;
    decltype(&getCreateSynchronizationObject2FailCall) getCreateSynchronizationObject2FailCallFcn = nullptr;
    decltype(&getFailOnSetContextSchedulingPriorityCall) getFailOnSetContextSchedulingPriorityCallFcn = nullptr;
    decltype(&getSetContextSchedulingPriorityDataCall) getSetContextSchedulingPriorityDataCallFcn = nullptr;
    decltype(&getRegisterTrimNotificationFailCall) getRegisterTrimNotificationFailCallFcn = nullptr;
    decltype(&getLastPriority) getLastPriorityFcn = nullptr;
    decltype(&setAdapterBDF) setAdapterBDFFcn = nullptr;
    decltype(&setMockDeviceExecutionState) setMockDeviceExecutionStateFcn = nullptr;
    decltype(&setMockGetDeviceStateReturnValue) setMockGetDeviceStateReturnValueFcn = nullptr;
    decltype(&setCapturingCreateAllocationFlags) setCapturingCreateAllocationFlagsFcn = nullptr;
    decltype(&getCapturedCreateAllocationFlags) getCapturedCreateAllocationFlagsFcn = nullptr;
    decltype(&setSupportCreateAllocationWithReadWriteExisitingSysMemory) setSupportCreateAllocationWithReadWriteExisitingSysMemoryFcn = nullptr;
};
