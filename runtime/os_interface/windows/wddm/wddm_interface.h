/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_info.h"
#include "core/os_interface/windows/windows_wrapper.h"
#include "runtime/os_interface/os_context.h"

#include <d3dkmthk.h>

#include <cstdint>

namespace NEO {
class Gdi;
class Wddm;
class OsContextWin;
class WddmResidencyController;
struct MonitoredFence;
struct WddmSubmitArguments;

class WddmInterface {
  public:
    WddmInterface(Wddm &wddm) : wddm(wddm){};
    virtual ~WddmInterface() = default;
    WddmInterface() = delete;
    virtual bool createHwQueue(OsContextWin &osContext) = 0;
    virtual void destroyHwQueue(D3DKMT_HANDLE hwQueue) = 0;
    virtual bool createMonitoredFence(OsContextWin &osContext) = 0;
    MOCKABLE_VIRTUAL bool createMonitoredFence(MonitoredFence &monitorFence);
    void destroyMonitorFence(D3DKMT_HANDLE fenceHandle);
    virtual void destroyMonitorFence(MonitoredFence &monitorFence) = 0;
    virtual const bool hwQueuesSupported() = 0;
    virtual bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) = 0;
    Wddm &wddm;
};

class WddmInterface20 : public WddmInterface {
  public:
    using WddmInterface::WddmInterface;
    bool createHwQueue(OsContextWin &osContext) override;
    void destroyHwQueue(D3DKMT_HANDLE hwQueue) override;
    bool createMonitoredFence(OsContextWin &osContext) override;
    void destroyMonitorFence(MonitoredFence &monitorFence) override;
    const bool hwQueuesSupported() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) override;
};

class WddmInterface23 : public WddmInterface {
  public:
    using WddmInterface::WddmInterface;
    bool createHwQueue(OsContextWin &osContext) override;
    void destroyHwQueue(D3DKMT_HANDLE hwQueue) override;
    bool createMonitoredFence(OsContextWin &osContext) override;
    void destroyMonitorFence(MonitoredFence &monitorFence) override;
    const bool hwQueuesSupported() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) override;
};
} // namespace NEO
