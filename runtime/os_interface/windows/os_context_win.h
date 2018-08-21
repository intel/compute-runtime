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

#pragma once
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "runtime/os_interface/windows/windows_defs.h"
#include <d3dkmthk.h>

namespace OCLRT {
class Wddm;

class OsContextWin {
  public:
    OsContextWin() = delete;
    OsContextWin(Wddm &wddm);
    ~OsContextWin();
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

  protected:
    bool initialized = false;
    D3DKMT_HANDLE context = 0;
    D3DKMT_HANDLE hwQueueHandle = 0;
    Wddm &wddm;
    MonitoredFence monitoredFence = {};
};
} // namespace OCLRT
