/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <cstdint>

namespace NEO {
class Gdi;
class Wddm;
class OsContextWin;
class WddmResidencyController;

class WddmInterface {
  public:
    WddmInterface(Wddm &wddm) : wddm(wddm){};
    virtual ~WddmInterface() = default;
    WddmInterface() = delete;
    virtual bool createHwQueue(OsContextWin &osContext) = 0;
    virtual void destroyHwQueue(D3DKMT_HANDLE hwQueue) = 0;
    bool createMonitoredFence(WddmResidencyController &residencyController);
    virtual const bool hwQueuesSupported() = 0;
    virtual bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) = 0;
    Wddm &wddm;
};

class WddmInterface20 : public WddmInterface {
  public:
    using WddmInterface::WddmInterface;
    bool createHwQueue(OsContextWin &osContext) override;
    void destroyHwQueue(D3DKMT_HANDLE hwQueue) override;
    const bool hwQueuesSupported() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) override;
};

class WddmInterface23 : public WddmInterface {
  public:
    using WddmInterface::WddmInterface;
    bool createHwQueue(OsContextWin &osContext) override;
    void destroyHwQueue(D3DKMT_HANDLE hwQueue) override;
    const bool hwQueuesSupported() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) override;
};
} // namespace NEO
