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

#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/wddm/wddm_interface.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

bool OCLRT::WddmInterface20::createHwQueue(PreemptionMode preemptionMode) {
    return false;
}

bool OCLRT::WddmInterface20::createMonitoredFence() {
    NTSTATUS Status;
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 CreateSynchronizationObject = {0};
    CreateSynchronizationObject.hDevice = wddm.getDevice();
    CreateSynchronizationObject.Info.Type = D3DDDI_MONITORED_FENCE;
    CreateSynchronizationObject.Info.MonitoredFence.InitialFenceValue = 0;

    Status = wddm.getGdi()->createSynchronizationObject2(&CreateSynchronizationObject);

    DEBUG_BREAK_IF(STATUS_SUCCESS != Status);

    wddm.resetMonitoredFenceParams(CreateSynchronizationObject.hSyncObject,
                                   reinterpret_cast<uint64_t *>(CreateSynchronizationObject.Info.MonitoredFence.FenceValueCPUVirtualAddress),
                                   CreateSynchronizationObject.Info.MonitoredFence.FenceValueGPUVirtualAddress);

    return Status == STATUS_SUCCESS;
}

const bool OCLRT::WddmInterface20::hwQueuesSupported() {
    return false;
}

bool OCLRT::WddmInterface20::submit(uint64_t commandBuffer, size_t size, void *commandHeader) {
    D3DKMT_SUBMITCOMMAND SubmitCommand = {0};
    NTSTATUS status = STATUS_SUCCESS;

    auto monitoredFence = wddm.getMonitoredFence();
    SubmitCommand.Commands = commandBuffer;
    SubmitCommand.CommandLength = static_cast<UINT>(size);
    SubmitCommand.BroadcastContextCount = 1;
    SubmitCommand.BroadcastContext[0] = wddm.getOsDeviceContext();
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

bool OCLRT::WddmInterface23::createHwQueue(PreemptionMode preemptionMode) {
    D3DKMT_CREATEHWQUEUE createHwQueue = {};

    if (!wddm.getGdi()->setupHwQueueProcAddresses()) {
        return false;
    }

    createHwQueue.hHwContext = wddm.getOsDeviceContext();
    if (preemptionMode >= PreemptionMode::MidBatch) {
        createHwQueue.Flags.DisableGpuTimeout = wddm.readEnablePreemptionRegKey();
    }

    auto status = wddm.getGdi()->createHwQueue(&createHwQueue);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    hwQueueHandle = createHwQueue.hHwQueue;

    wddm.resetMonitoredFenceParams(createHwQueue.hHwQueueProgressFence,
                                   reinterpret_cast<uint64_t *>(createHwQueue.HwQueueProgressFenceCPUVirtualAddress),
                                   createHwQueue.HwQueueProgressFenceGPUVirtualAddress);

    return status == STATUS_SUCCESS;
}

void OCLRT::WddmInterface23::destroyHwQueue() {
    if (hwQueueHandle) {
        D3DKMT_DESTROYHWQUEUE destroyHwQueue = {};
        destroyHwQueue.hHwQueue = hwQueueHandle;

        auto status = wddm.getGdi()->destroyHwQueue(&destroyHwQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }
}

bool OCLRT::WddmInterface23::createMonitoredFence() {
    return true;
}

const bool OCLRT::WddmInterface23::hwQueuesSupported() {
    return true;
}

bool OCLRT::WddmInterface23::submit(uint64_t commandBuffer, size_t size, void *commandHeader) {
    auto monitoredFence = wddm.getMonitoredFence();

    D3DKMT_SUBMITCOMMANDTOHWQUEUE submitCommand = {};
    submitCommand.hHwQueue = hwQueueHandle;
    submitCommand.HwQueueProgressFenceId = monitoredFence.fenceHandle;
    submitCommand.CommandBuffer = commandBuffer;
    submitCommand.CommandLength = static_cast<UINT>(size);

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);
    pHeader->MonitorFenceVA = monitoredFence.gpuAddress;
    pHeader->MonitorFenceValue = monitoredFence.currentFenceValue;

    submitCommand.pPrivateDriverData = commandHeader;
    submitCommand.PrivateDriverDataSize = sizeof(COMMAND_BUFFER_HEADER);

    auto status = wddm.getGdi()->submitCommandToHwQueue(&submitCommand);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    return status == STATUS_SUCCESS;
}
