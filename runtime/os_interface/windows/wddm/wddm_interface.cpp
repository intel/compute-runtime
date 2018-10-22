/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/wddm/wddm_interface.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/os_context_win.h"

using namespace OCLRT;

bool WddmInterface20::createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) {
    return false;
}
void WddmInterface20::destroyHwQueue(D3DKMT_HANDLE hwQueue) {}

bool WddmInterface::createMonitoredFence(OsContextWin &osContext) {
    NTSTATUS Status;
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 CreateSynchronizationObject = {0};
    CreateSynchronizationObject.hDevice = wddm.getDevice();
    CreateSynchronizationObject.Info.Type = D3DDDI_MONITORED_FENCE;
    CreateSynchronizationObject.Info.MonitoredFence.InitialFenceValue = 0;

    Status = wddm.getGdi()->createSynchronizationObject2(&CreateSynchronizationObject);

    DEBUG_BREAK_IF(STATUS_SUCCESS != Status);

    osContext.getResidencyController().resetMonitoredFenceParams(CreateSynchronizationObject.hSyncObject,
                                                                 reinterpret_cast<uint64_t *>(CreateSynchronizationObject.Info.MonitoredFence.FenceValueCPUVirtualAddress),
                                                                 CreateSynchronizationObject.Info.MonitoredFence.FenceValueGPUVirtualAddress);

    return Status == STATUS_SUCCESS;
}

const bool WddmInterface20::hwQueuesSupported() {
    return false;
}

bool WddmInterface20::submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) {
    D3DKMT_SUBMITCOMMAND SubmitCommand = {0};
    NTSTATUS status = STATUS_SUCCESS;

    auto monitoredFence = osContext.getResidencyController().getMonitoredFence();
    SubmitCommand.Commands = commandBuffer;
    SubmitCommand.CommandLength = static_cast<UINT>(size);
    SubmitCommand.BroadcastContextCount = 1;
    SubmitCommand.BroadcastContext[0] = osContext.getContext();
    SubmitCommand.Flags.NullRendering = (UINT)DebugManager.flags.EnableNullHardware.get();

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    pHeader->MonitorFenceVA = monitoredFence.gpuAddress;
    pHeader->MonitorFenceValue = monitoredFence.currentFenceValue;

    // Note: Private data should be the CPU VA Address
    SubmitCommand.pPrivateDriverData = commandHeader;
    SubmitCommand.PrivateDriverDataSize = sizeof(COMMAND_BUFFER_HEADER);

    status = wddm.getGdi()->submitCommand(&SubmitCommand);

    return STATUS_SUCCESS == status;
}

bool WddmInterface23::createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) {
    D3DKMT_CREATEHWQUEUE createHwQueue = {};

    if (!wddm.getGdi()->setupHwQueueProcAddresses()) {
        return false;
    }

    createHwQueue.hHwContext = osContext.getContext();
    if (preemptionMode >= PreemptionMode::MidBatch) {
        createHwQueue.Flags.DisableGpuTimeout = wddm.readEnablePreemptionRegKey();
    }

    auto status = wddm.getGdi()->createHwQueue(&createHwQueue);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    osContext.setHwQueue(createHwQueue.hHwQueue);

    return status == STATUS_SUCCESS;
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

bool WddmInterface23::submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) {
    auto monitoredFence = osContext.getResidencyController().getMonitoredFence();

    D3DKMT_SUBMITCOMMANDTOHWQUEUE submitCommand = {};
    submitCommand.hHwQueue = osContext.getHwQueue();
    submitCommand.HwQueueProgressFenceId = monitoredFence.fenceHandle;
    submitCommand.CommandBuffer = commandBuffer;
    submitCommand.CommandLength = static_cast<UINT>(size);

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);
    pHeader->MonitorFenceVA = monitoredFence.gpuAddress;
    pHeader->MonitorFenceValue = monitoredFence.currentFenceValue;

    submitCommand.pPrivateDriverData = commandHeader;
    submitCommand.PrivateDriverDataSize = MemoryConstants::pageSize;

    auto status = wddm.getGdi()->submitCommandToHwQueue(&submitCommand);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    return status == STATUS_SUCCESS;
}
