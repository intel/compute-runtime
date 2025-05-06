/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

using namespace NEO;

bool WddmInterface32::createSyncObject(MonitoredFence &monitorFence) {
    UNRECOVERABLE_IF(wddm.getGdi()->createNativeFence == nullptr);
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_CREATENATIVEFENCE createNativeFenceObject = {0};
    createNativeFenceObject.hDevice = wddm.getDeviceHandle();
    createNativeFenceObject.Info.Type = D3DDDI_NATIVEFENCE_TYPE_DEFAULT;
    createNativeFenceObject.Info.InitialFenceValue = 0;

    status = wddm.getGdi()->createNativeFence(&createNativeFenceObject);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);

    monitorFence.fenceHandle = createNativeFenceObject.hSyncObject;
    monitorFence.cpuAddress = reinterpret_cast<uint64_t *>(createNativeFenceObject.Info.NativeFenceMapping.CurrentValueCpuVa);
    monitorFence.gpuAddress = createNativeFenceObject.Info.NativeFenceMapping.CurrentValueGpuVa;

    return status == STATUS_SUCCESS;
}
