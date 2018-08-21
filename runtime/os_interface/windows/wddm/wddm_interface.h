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
#include <d3dkmthk.h>
#include <cstdint>
#include "runtime/helpers/hw_info.h"

namespace OCLRT {
class Gdi;
class OsContextWin;
class Wddm;
class WddmInterface {
  public:
    WddmInterface(Wddm &wddm) : wddm(wddm){};
    virtual ~WddmInterface() = default;
    WddmInterface() = delete;
    virtual bool createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) = 0;
    virtual void destroyHwQueue(D3DKMT_HANDLE hwQueue) = 0;
    virtual bool createMonitoredFence(OsContextWin &osContext) = 0;
    virtual const bool hwQueuesSupported() = 0;
    virtual bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) = 0;
    Wddm &wddm;
};

class WddmInterface20 : public WddmInterface {
  public:
    using WddmInterface::WddmInterface;
    bool createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) override;
    void destroyHwQueue(D3DKMT_HANDLE hwQueue) override;
    bool createMonitoredFence(OsContextWin &osContext) override;
    const bool hwQueuesSupported() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) override;
};

class WddmInterface23 : public WddmInterface {
  public:
    using WddmInterface::WddmInterface;
    bool createHwQueue(PreemptionMode preemptionMode, OsContextWin &osContext) override;
    void destroyHwQueue(D3DKMT_HANDLE hwQueue) override;
    bool createMonitoredFence(OsContextWin &osContext) override;
    const bool hwQueuesSupported() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) override;
};
} // namespace OCLRT
