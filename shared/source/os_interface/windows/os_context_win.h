/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"

namespace NEO {

class Wddm;

struct HardwareQueue {
    D3DKMT_HANDLE handle = 0;
    D3DKMT_HANDLE progressFenceHandle = 0;
    VOID *progressFenceCpuVA = nullptr;
    D3DGPU_VIRTUAL_ADDRESS progressFenceGpuVA = 0;
};

class OsContextWin : public OsContext {
  public:
    OsContextWin() = delete;
    ~OsContextWin() override;

    OsContextWin(Wddm &wddm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);

    D3DKMT_HANDLE getWddmContextHandle() const { return wddmContextHandle; }
    void setWddmContextHandle(D3DKMT_HANDLE wddmContextHandle) { this->wddmContextHandle = wddmContextHandle; }
    HardwareQueue getHwQueue() const { return hardwareQueue; }
    void setHwQueue(HardwareQueue hardwareQueue) { this->hardwareQueue = hardwareQueue; }
    bool isDirectSubmissionSupported() const override;
    Wddm *getWddm() const { return &wddm; }
    MOCKABLE_VIRTUAL WddmResidencyController &getResidencyController();
    static OsContext *create(OSInterface *osInterface, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);
    MonitoredFence &getMonitoredFence() { return monitoredFence; }
    void resetMonitoredFenceParams(D3DKMT_HANDLE &handle, uint64_t *cpuAddress, D3DGPU_VIRTUAL_ADDRESS &gpuAddress);
    bool wasAllocationUsedSinceLastTrim(uint64_t fenceValue) { return fenceValue > lastTrimFenceValue; }
    void updateLastTrimFenceValue() { lastTrimFenceValue = *monitoredFence.cpuAddress; }
    uint64_t getLastTrimFenceValue() const { return this->lastTrimFenceValue; };
    void reInitializeContext() override;
    void getDeviceLuidArray(std::vector<uint8_t> &luidData, size_t arraySize);
    uint32_t getDeviceNodeMask();
    uint64_t getOfflineDumpContextId(uint32_t deviceIndex) const override;

  protected:
    bool initializeContext(bool allocateInterrupt) override;

    HardwareQueue hardwareQueue;

    MonitoredFence monitoredFence = {};
    uint64_t lastTrimFenceValue = 0u;

    Wddm &wddm;
    D3DKMT_HANDLE wddmContextHandle = 0;
};
} // namespace NEO
