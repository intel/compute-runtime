/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/windows/wddm_residency_controller.h"

namespace NEO {

class Wddm;
class OsContextWin : public OsContext {
  public:
    OsContextWin() = delete;
    ~OsContextWin() override;

    OsContextWin(Wddm &wddm, uint32_t contextId, DeviceBitfield deviceBitfield,
                 aub_stream::EngineType engineType, PreemptionMode preemptionMode, bool lowPriority);

    D3DKMT_HANDLE getWddmContextHandle() const { return wddmContextHandle; }
    void setWddmContextHandle(D3DKMT_HANDLE wddmContextHandle) { this->wddmContextHandle = wddmContextHandle; }
    D3DKMT_HANDLE getHwQueue() const { return hwQueueHandle; }
    void setHwQueue(D3DKMT_HANDLE hwQueue) { hwQueueHandle = hwQueue; }
    bool isInitialized() const { return initialized; }
    Wddm *getWddm() const { return &wddm; }
    WddmResidencyController &getResidencyController() { return residencyController; }

  protected:
    bool initialized = false;
    D3DKMT_HANDLE wddmContextHandle = 0;
    D3DKMT_HANDLE hwQueueHandle = 0;
    Wddm &wddm;
    WddmResidencyController residencyController;
};
} // namespace NEO
