/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/system_info.h"

#include "drm/i915_drm.h"
#include "drm/intel_hwconfig_types.h"

namespace NEO {
struct HardwareInfo;
struct SystemInfoImpl : public SystemInfo {
    ~SystemInfoImpl() override = default;

    SystemInfoImpl(const uint32_t *data, int32_t length);

    uint32_t getMaxSlicesSupported() const override { return maxSlicesSupported; }
    uint32_t getMaxDualSubSlicesSupported() const override { return maxDualSubSlicesSupported; }
    uint32_t getMaxEuPerDualSubSlice() const override { return maxEuPerDualSubSlice; }
    uint64_t getL3CacheSizeInKb() const override { return L3CacheSizeInKb; }
    uint32_t getL3BankCount() const override { return L3BankCount; }
    uint32_t getNumThreadsPerEu() const override { return numThreadsPerEu; }
    uint32_t getTotalVsThreads() const override { return totalVsThreads; }
    uint32_t getTotalHsThreads() const override { return totalHsThreads; }
    uint32_t getTotalDsThreads() const override { return totalDsThreads; }
    uint32_t getTotalGsThreads() const override { return totalGsThreads; }
    uint32_t getTotalPsThreads() const override { return totalPsThreads; }
    uint32_t getMaxFillRate() const override { return maxFillRate; }
    uint32_t getMaxRCS() const override { return maxRCS; }
    uint32_t getMaxCCS() const override { return maxCCS; }
    void checkSysInfoMismatch(HardwareInfo *hwInfo) override;

  protected:
    void parseDeviceBlob(const uint32_t *data, int32_t size);

    uint32_t maxSlicesSupported = 0;
    uint32_t maxDualSubSlicesSupported = 0;
    uint32_t maxEuPerDualSubSlice = 0;
    uint64_t L3CacheSizeInKb = 0;
    uint32_t L3BankCount = 0;
    uint32_t numThreadsPerEu = 0;
    uint32_t totalVsThreads = 0;
    uint32_t totalHsThreads = 0;
    uint32_t totalDsThreads = 0;
    uint32_t totalGsThreads = 0;
    uint32_t totalPsThreads = 0;
    uint32_t maxFillRate = 0;
    uint32_t maxRCS = 0;
    uint32_t maxCCS = 0;
};

} // namespace NEO
