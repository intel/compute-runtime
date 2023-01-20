/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

NTSTATUS destroyAllocationNoOp(const D3DKMT_DESTROYALLOCATION2 *allocStruct) {
    return STATUS_SUCCESS;
}

NTSTATUS waitForSynchronizationObjectFromCpuNoOp(const D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *waitStruct) {
    return STATUS_SUCCESS;
}

NTSTATUS destroyPagingQueueNoOp(D3DDDI_DESTROYPAGINGQUEUE *destroyPagingQueue) {
    return STATUS_SUCCESS;
}

NTSTATUS destroyDeviceNoOp(const D3DKMT_DESTROYDEVICE *destroyDevice) {
    return STATUS_SUCCESS;
}

NTSTATUS closeAdapterNoOp(const D3DKMT_CLOSEADAPTER *closeAdapter) {
    return STATUS_SUCCESS;
}

bool Wddm::isDriverAvailable() {
    D3DKMT_GETDEVICESTATE deviceState = {};
    deviceState.hDevice = device;
    deviceState.StateType = D3DKMT_DEVICESTATE_PRESENT;

    NTSTATUS status = STATUS_SUCCESS;
    status = getGdi()->getDeviceState(&deviceState);
    if (status != STATUS_SUCCESS) {
        skipResourceCleanupVar = true;
        getGdi()->destroyAllocation2 = &destroyAllocationNoOp;
        getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOp;
        getGdi()->destroyPagingQueue = &destroyPagingQueueNoOp;
        getGdi()->destroyDevice = &destroyDeviceNoOp;
        getGdi()->closeAdapter = &closeAdapterNoOp;
    }
    return status == STATUS_SUCCESS;
}

} // namespace NEO
