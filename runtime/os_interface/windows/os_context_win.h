/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/windows/wddm_residency_controller.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "runtime/os_interface/windows/windows_defs.h"
#include <d3dkmthk.h>

namespace OCLRT {

class Wddm;
using OsContextWin = OsContext::OsContextImpl;
class OsContext::OsContextImpl {
  public:
    OsContextImpl() = delete;
    OsContextImpl(Wddm &wddm, uint32_t osContextId);
    ~OsContextImpl();
    D3DKMT_HANDLE getContext() const {
        return context;
    }
    D3DKMT_HANDLE getHwQueue() const {
        return hwQueueHandle;
    }
    void setHwQueue(D3DKMT_HANDLE hwQueue) {
        hwQueueHandle = hwQueue;
    }
    bool isInitialized() const {
        return initialized;
    }
    MonitoredFence &getMonitoredFence() { return monitoredFence; }
    void resetMonitoredFenceParams(D3DKMT_HANDLE &handle, uint64_t *cpuAddress, D3DGPU_VIRTUAL_ADDRESS &gpuAddress);
    Wddm *getWddm() const { return &wddm; }
    WddmResidencyController &getResidencyController() { return residencyController; }

  protected:
    bool initialized = false;
    D3DKMT_HANDLE context = 0;
    D3DKMT_HANDLE hwQueueHandle = 0;
    Wddm &wddm;
    MonitoredFence monitoredFence = {};
    WddmResidencyController residencyController;
};
} // namespace OCLRT
