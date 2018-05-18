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
#include "runtime/os_interface/windows/wddm/wddm23.h"

namespace OCLRT {
Wddm23::Wddm23() : Wddm20() {}

Wddm23::~Wddm23() {
    destroyHwQueue();
}

bool Wddm23::createHwQueue() {
    D3DKMT_CREATEHWQUEUE createHwQueue = {};

    if (!gdi->setupHwQueueProcAddresses()) {
        return false;
    }

    createHwQueue.hHwContext = context;
    if (preemptionMode >= PreemptionMode::MidBatch) {
        createHwQueue.Flags.DisableGpuTimeout = readEnablePreemptionRegKey();
    }

    auto status = gdi->createHwQueue(&createHwQueue);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    hwQueueHandle = createHwQueue.hHwQueue;

    resetMonitoredFenceParams(createHwQueue.hHwQueueProgressFence,
                              reinterpret_cast<uint64_t *>(createHwQueue.HwQueueProgressFenceCPUVirtualAddress),
                              createHwQueue.HwQueueProgressFenceGPUVirtualAddress);

    return status == STATUS_SUCCESS;
}

void Wddm23::destroyHwQueue() {
    if (hwQueueHandle) {
        D3DKMT_DESTROYHWQUEUE destroyHwQueue = {};
        destroyHwQueue.hHwQueue = hwQueueHandle;

        auto status = gdi->destroyHwQueue(&destroyHwQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }
}

bool Wddm23::submit(uint64_t commandBuffer, size_t size, void *commandHeader) {
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

    if (currentPagingFenceValue > *pagingFenceAddress && !waitOnGPU()) {
        return false;
    }

    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", monitoredFence.currentFenceValue);

    auto status = gdi->submitCommandToHwQueue(&submitCommand);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);

    if (STATUS_SUCCESS == status) {
        monitoredFence.lastSubmittedFence = monitoredFence.currentFenceValue;
        monitoredFence.currentFenceValue++;
    }

    getDeviceState();

    return status == STATUS_SUCCESS;
}
} // namespace OCLRT
