/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_temp_storage.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

using namespace NEO;

bool WddmInterface::createMonitoredFence(MonitoredFence &monitorFence) {
    NTSTATUS status = STATUS_SUCCESS;
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 createSynchronizationObject = {0};
    createSynchronizationObject.hDevice = wddm.getDeviceHandle();
    createSynchronizationObject.Info.Type = D3DDDI_MONITORED_FENCE;
    createSynchronizationObject.Info.MonitoredFence.InitialFenceValue = 0;

    status = wddm.getGdi()->createSynchronizationObject2(&createSynchronizationObject);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);

    monitorFence.fenceHandle = createSynchronizationObject.hSyncObject;
    monitorFence.cpuAddress = reinterpret_cast<uint64_t *>(createSynchronizationObject.Info.MonitoredFence.FenceValueCPUVirtualAddress);
    monitorFence.gpuAddress = createSynchronizationObject.Info.MonitoredFence.FenceValueGPUVirtualAddress;

    return status == STATUS_SUCCESS;
}
void WddmInterface::destroyMonitorFence(D3DKMT_HANDLE fenceHandle) {
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySyncObject = {0};
    destroySyncObject.hSyncObject = fenceHandle;
    [[maybe_unused]] NTSTATUS status = wddm.getGdi()->destroySynchronizationObject(&destroySyncObject);
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

bool WddmInterface20::hwQueuesSupported() {
    return false;
}

bool WddmInterface20::submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) {
    D3DKMT_SUBMITCOMMAND submitCommand = {0};
    NTSTATUS status = STATUS_SUCCESS;

    submitCommand.Commands = commandBuffer;
    submitCommand.CommandLength = static_cast<UINT>(size);
    submitCommand.BroadcastContextCount = 1;
    submitCommand.BroadcastContext[0] = submitArguments.contextHandle;
    submitCommand.Flags.NullRendering = (UINT)debugManager.flags.EnableNullHardware.get();

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);

    pHeader->MonitorFenceVA = submitArguments.monitorFence->gpuAddress;
    pHeader->MonitorFenceValue = submitArguments.monitorFence->currentFenceValue;

    // Note: Private data should be the CPU VA Address
    UmKmDataTempStorage<COMMAND_BUFFER_HEADER> internalRepresentation;
    if (wddm.getHwDeviceId()->getUmKmDataTranslator()->enabled()) {
        internalRepresentation.resize(wddm.getHwDeviceId()->getUmKmDataTranslator()->getSizeForCommandBufferHeaderDataInternalRepresentation());
        bool translated = wddm.getHwDeviceId()->getUmKmDataTranslator()->translateCommandBufferHeaderDataToInternalRepresentation(internalRepresentation.data(), internalRepresentation.size(), *pHeader);
        UNRECOVERABLE_IF(false == translated);
        submitCommand.pPrivateDriverData = internalRepresentation.data();
        submitCommand.PrivateDriverDataSize = static_cast<uint32_t>(internalRepresentation.size());
    } else {
        submitCommand.pPrivateDriverData = pHeader;
        submitCommand.PrivateDriverDataSize = sizeof(COMMAND_BUFFER_HEADER);
    }

    status = wddm.getGdi()->submitCommand(&submitCommand);

    return STATUS_SUCCESS == status;
}

bool NEO::WddmInterface20::createFenceForDirectSubmission(MonitoredFence &monitorFence, OsContextWin &osContext) {
    auto ret = WddmInterface::createMonitoredFence(monitorFence);
    monitorFence.currentFenceValue = 1;
    return ret;
}

bool WddmInterface23::createHwQueue(OsContextWin &osContext) {
    D3DKMT_CREATEHWQUEUE createHwQueue = {};
    CREATEHWQUEUE_PVTDATA hwQueuePrivateData = {};

    if (!wddm.getGdi()->setupHwQueueProcAddresses()) {
        return false;
    }

    if (osContext.isPartOfContextGroup()) {
        hwQueuePrivateData = initHwQueuePrivateData(osContext);
        createHwQueue.pPrivateDriverData = &hwQueuePrivateData;
        createHwQueue.PrivateDriverDataSize = sizeof(hwQueuePrivateData);
    }

    createHwQueue.hHwContext = osContext.getWddmContextHandle();
    if (osContext.getPreemptionMode() >= PreemptionMode::MidBatch) {
        createHwQueue.Flags.DisableGpuTimeout = wddm.getEnablePreemptionRegValue();
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

        [[maybe_unused]] auto status = wddm.getGdi()->destroyHwQueue(&destroyHwQueue);
        DEBUG_BREAK_IF(status != STATUS_SUCCESS);
    }
}

bool WddmInterface23::hwQueuesSupported() {
    return true;
}

bool WddmInterface23::submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) {
    D3DKMT_SUBMITCOMMANDTOHWQUEUE submitCommand = {};
    submitCommand.hHwQueue = submitArguments.hwQueueHandle;
    submitCommand.HwQueueProgressFenceId = submitArguments.monitorFence->currentFenceValue;
    submitCommand.CommandBuffer = commandBuffer;
    submitCommand.CommandLength = static_cast<UINT>(size);

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandHeader);
    UmKmDataTempStorage<COMMAND_BUFFER_HEADER> internalRepresentation;
    if (wddm.getHwDeviceId()->getUmKmDataTranslator()->enabled()) {
        internalRepresentation.resize(wddm.getHwDeviceId()->getUmKmDataTranslator()->getSizeForCommandBufferHeaderDataInternalRepresentation());
        bool translated = wddm.getHwDeviceId()->getUmKmDataTranslator()->translateCommandBufferHeaderDataToInternalRepresentation(internalRepresentation.data(), internalRepresentation.size(), *pHeader);
        UNRECOVERABLE_IF(false == translated);
        submitCommand.pPrivateDriverData = internalRepresentation.data();
        submitCommand.PrivateDriverDataSize = static_cast<uint32_t>(internalRepresentation.size());
    } else {
        submitCommand.pPrivateDriverData = pHeader;
        submitCommand.PrivateDriverDataSize = sizeof(COMMAND_BUFFER_HEADER);
    }

    if (!debugManager.flags.UseCommandBufferHeaderSizeForWddmQueueSubmission.get()) {
        submitCommand.PrivateDriverDataSize = MemoryConstants::pageSize;
    }

    auto status = wddm.getGdi()->submitCommandToHwQueue(&submitCommand);
    return status == STATUS_SUCCESS;
}

bool NEO::WddmInterface23::createFenceForDirectSubmission(MonitoredFence &monitorFence, OsContextWin &osContext) {
    MonitoredFence monitorFenceForResidency{};
    auto ret = createSyncObject(monitorFenceForResidency);
    auto &residencyController = osContext.getResidencyController();
    auto lastSubmittedFence = residencyController.getMonitoredFence().lastSubmittedFence;
    auto currentFenceValue = residencyController.getMonitoredFence().currentFenceValue;
    residencyController.resetMonitoredFenceParams(monitorFenceForResidency.fenceHandle,
                                                  const_cast<uint64_t *>(monitorFenceForResidency.cpuAddress),
                                                  monitorFenceForResidency.gpuAddress);
    residencyController.getMonitoredFence().currentFenceValue = currentFenceValue;
    residencyController.getMonitoredFence().lastSubmittedFence = lastSubmittedFence;

    auto hwQueue = osContext.getHwQueue();
    monitorFence.cpuAddress = reinterpret_cast<uint64_t *>(hwQueue.progressFenceCpuVA);
    monitorFence.currentFenceValue = currentFenceValue;
    monitorFence.lastSubmittedFence = lastSubmittedFence;
    monitorFence.gpuAddress = hwQueue.progressFenceGpuVA;
    monitorFence.fenceHandle = hwQueue.progressFenceHandle;

    return ret;
}

bool WddmInterface23::createSyncObject(MonitoredFence &monitorFence) {
    auto ret = WddmInterface::createMonitoredFence(monitorFence);
    return ret;
}
