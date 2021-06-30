/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/system_info.h"

namespace NEO {
struct HardwareInfo;
struct SystemInfoImpl : public SystemInfo {
    ~SystemInfoImpl() override = default;

    SystemInfoImpl(const uint32_t *data, int32_t length) {
    }

    uint32_t getMaxSlicesSupported() const override { return 0; }
    uint32_t getMaxDualSubSlicesSupported() const override { return 0; }
    uint32_t getMaxEuPerDualSubSlice() const override { return 0; }
    uint64_t getL3CacheSizeInKb() const override { return 0; }
    uint32_t getL3BankCount() const override { return 0; }
    uint32_t getMemoryType() const override { return 0; }
    uint32_t getMaxMemoryChannels() const override { return 0; }
    uint32_t getNumThreadsPerEu() const override { return 0; }
    uint32_t getTotalVsThreads() const override { return 0; }
    uint32_t getTotalHsThreads() const override { return 0; }
    uint32_t getTotalDsThreads() const override { return 0; }
    uint32_t getTotalGsThreads() const override { return 0; }
    uint32_t getTotalPsThreads() const override { return 0; }
    uint32_t getMaxFillRate() const override { return 0; }
    uint32_t getMaxRCS() const override { return 0; }
    uint32_t getMaxCCS() const override { return 0; }
    void checkSysInfoMismatch(HardwareInfo *hwInfo) override {}
};

} // namespace NEO
