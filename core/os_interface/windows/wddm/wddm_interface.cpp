/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/windows/wddm/wddm_interface.h"

#include "core/memory_manager/memory_constants.h"
#include "core/os_interface/windows/gdi_interface.h"
#include "core/os_interface/windows/os_context_win.h"
#include "core/os_interface/windows/wddm/wddm.h"

using namespace NEO;

bool WddmInterface::createMonitoredFence(MonitoredFence &monitorFence) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 CreateSynchronizationObject = {0};
    CreateSynchronizationObject.hDevice = wddm.getDevice();
    CreateSynchronizationObject.Info.Type = D3DDDI_MONITORED_FENCE;
    CreateSynchronizationObject.Info.MonitoredFence.InitialFenceValue = 0;

    status = wddm.getGdi()->createSynchronizationObject2(&CreateSynchronizationObject);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);

    monitorFence.fenceHandle = CreateSynchronizationObject.hSyncObject;
    monitorFence.cpuAddress = reinterpret_cast<uint64_t *>(CreateSynchronizationObject.Info.MonitoredFence.FenceValueCPUVirtualAddress);
    monitorFence.gpuAddress = CreateSynchronizationObject.Info.MonitoredFence.FenceValueGPUVirtualAddress;

    return status == STATUS_SUCCESS;
}
void WddmInterface::destroyMonitorFence(D3DKMT_HANDLE fenceHandle) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySyncObject = {0};
    destroySyncObject.hSyncObject = fenceHandle;
    status = wddm.getGdi()->destroySynchronizationObject(&destroySyncObject);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);
}

bool WddmInterface20::createHwQueue(OsContextWin &osContext) {
    return false;
}
void WddmInterface20::destroyHwQueue(D3DKMT_HANDLE hwQueue) {}

bool WddmInterface20::createMonitoredFence(OsContextWin &osContext) {
    auto &residencyController = osContext.getResidencyController();
    MonitoredFence &monitorFence = residencyController.getMonitoredFence();
    bool ret = WddmInterface::createMonitoredFence(monitorFence);

    monitorFence.currentFenceValue = 1;

    return ret;
}

void WddmInterface20::destroyMonitorFence(MonitoredFence &monitorFence) {
    WddmInterface::destroyMonitorFence(monitorFence.fenceHandle);
}

const bool WddmInterface20::hwQueuesSupported() {
    return false;
}

bool WddmInterface20::submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) {
    D3DKMT_SUBMITCOMMAND SubmitCommand = {0};
    NTSTATUS status = STATUS_SUCCESS;

    SubmitCommand.Commands = commandBuffer;
    SubmitCommand.CommandLength = static_cast<UINT>(size);
    SubmitCommand.BroadcastContextCount = 1;
    SubmitCommand.BroadcastContext[0] = submitArguments.contextHandle;
    SubmitCommand.Flags.NullRendering = (UINT)DebugManager.flags.EnableNullHardware.get();

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    pHeader->MonitorFenceVA = submitArguments.monitorFence->gpuAddress;
    pHeader->MonitorFenceValue = submitArguments.monitorFence->currentFenceValue;

    // Note: Private data should be the CPU VA Address
    SubmitCommand.pPrivateDriverData = commandHeader;
    SubmitCommand.PrivateDriverDataSize = sizeof(COMMAND_BUFFER_HEADER);

    status = wddm.getGdi()->submitCommand(&SubmitCommand);

    return STATUS_SUCCESS == status;
}

bool WddmInterface23::createHwQueue(OsContextWin &osContext) {
    D3DKMT_CREATEHWQUEUE createHwQueue = {};

    if (!wddm.getGdi()->setupHwQueueProcAddresses()) {
        return false;
    }

    createHwQueue.hHwContext = osContext.getWddmContextHandle();
    if (osContext.getPreemptionMode() >= PreemptionMode::MidBatch) {
        createHwQueue.Flags.DisableGpuTimeout = wddm.readEnablePreemptionRegKey();
    }

    auto status = wddm.getGdi()->createHwQueue(&createHwQueue);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    osContext.setHwQueue({createHwQueue.hHwQueue, createHwQueue.hHwQueueProgressFence, createHwQueue.HwQueueProgressFenceCPUVirtualAddress,
                          createHwQueue.HwQueueProgressFenceGPUVirtualAddress});

    return status == STATUS_SUCCESS;
}

bool WddmInterface23::createMonitoredFence(OsContextWin &osContext) {
    auto &residencyController = osContext.getResidencyController();
    auto hwQueue = osContext.getHwQueue();
    residencyController.resetMonitoredFenceParams(hwQueue.progressFenceHandle,
                                                  reinterpret_cast<uint64_t *>(hwQueue.progressFenceCpuVA),
                                                  hwQueue.progressFenceGpuVA);
    return true;
}

void WddmInterface23::destroyMonitorFence(MonitoredFence &monitorFence) {
}

void WddmInterface23::destroyHwQueue(D3DKMT_HANDLE hwQueue) {
    if (hwQueue) {
        D3DKMT_DESTROYHWQUEUE destroyHwQueue = {};
        destroyHwQueue.hHwQueue = hwQueue;

        auto status = wddm.getGdi()->destroyHwQueue(&destroyHwQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }
}

const bool WddmInterface23::hwQueuesSupported() {
    return true;
}

bool WddmInterface23::submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) {
    D3DKMT_SUBMITCOMMANDTOHWQUEUE submitCommand = {};
    submitCommand.hHwQueue = submitArguments.hwQueueHandle;
    submitCommand.HwQueueProgressFenceId = submitArguments.monitorFence->currentFenceValue;
    submitCommand.CommandBuffer = commandBuffer;
    submitCommand.CommandLength = static_cast<UINT>(size);

    submitCommand.pPrivateDriverData = commandHeader;
    submitCommand.PrivateDriverDataSize = MemoryConstants::pageSize;

    auto status = wddm.getGdi()->submitCommandToHwQueue(&submitCommand);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    return status == STATUS_SUCCESS;
}
