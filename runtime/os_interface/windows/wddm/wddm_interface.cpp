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
#include "runtime/os_interface/windows/os_context_win.h"

bool OCLRT::WddmInterface20::createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) {
    return false;
}
void OCLRT::WddmInterface20::destroyHwQueue(D3DKMT_HANDLE hwQueue) {}

bool OCLRT::WddmInterface20::createMonitoredFence(OsContextWin &osContext) {
    NTSTATUS Status;
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 CreateSynchronizationObject = {0};
    CreateSynchronizationObject.hDevice = wddm.getDevice();
    CreateSynchronizationObject.Info.Type = D3DDDI_MONITORED_FENCE;
    CreateSynchronizationObject.Info.MonitoredFence.InitialFenceValue = 0;

    Status = wddm.getGdi()->createSynchronizationObject2(&CreateSynchronizationObject);

    DEBUG_BREAK_IF(STATUS_SUCCESS != Status);

    osContext.resetMonitoredFenceParams(CreateSynchronizationObject.hSyncObject,
                                        reinterpret_cast<uint64_t *>(CreateSynchronizationObject.Info.MonitoredFence.FenceValueCPUVirtualAddress),
                                        CreateSynchronizationObject.Info.MonitoredFence.FenceValueGPUVirtualAddress);

    return Status == STATUS_SUCCESS;
}

const bool OCLRT::WddmInterface20::hwQueuesSupported() {
    return false;
}

bool OCLRT::WddmInterface20::submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) {
    D3DKMT_SUBMITCOMMAND SubmitCommand = {0};
    NTSTATUS status = STATUS_SUCCESS;

    auto monitoredFence = osContext.getMonitoredFence();
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

bool OCLRT::WddmInterface23::createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) {
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

    osContext.resetMonitoredFenceParams(createHwQueue.hHwQueueProgressFence,
                                        reinterpret_cast<uint64_t *>(createHwQueue.HwQueueProgressFenceCPUVirtualAddress),
                                        createHwQueue.HwQueueProgressFenceGPUVirtualAddress);

    return status == STATUS_SUCCESS;
}

void OCLRT::WddmInterface23::destroyHwQueue(D3DKMT_HANDLE hwQueue) {
    if (hwQueue) {
        D3DKMT_DESTROYHWQUEUE destroyHwQueue = {};
        destroyHwQueue.hHwQueue = hwQueue;

        auto status = wddm.getGdi()->destroyHwQueue(&destroyHwQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }
}

bool OCLRT::WddmInterface23::createMonitoredFence(OsContextWin &osContext) {
    return true;
}

const bool OCLRT::WddmInterface23::hwQueuesSupported() {
    return true;
}

bool OCLRT::WddmInterface23::submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) {
    auto monitoredFence = osContext.getMonitoredFence();

    D3DKMT_SUBMITCOMMANDTOHWQUEUE submitCommand = {};
    submitCommand.hHwQueue = osContext.getHwQueue();
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
