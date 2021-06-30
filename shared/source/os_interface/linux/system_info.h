/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
struct HardwareInfo;
struct SystemInfo {
    SystemInfo() = default;
    virtual ~SystemInfo() = 0;

    virtual uint32_t getMaxSlicesSupported() const = 0;
    virtual uint32_t getMaxDualSubSlicesSupported() const = 0;
    virtual uint32_t getMaxEuPerDualSubSlice() const = 0;
    virtual uint64_t getL3CacheSizeInKb() const = 0;
    virtual uint32_t getL3BankCount() const = 0;
    virtual uint32_t getMemoryType() const = 0;
    virtual uint32_t getMaxMemoryChannels() const = 0;
    virtual uint32_t getNumThreadsPerEu() const = 0;
    virtual uint32_t getTotalVsThreads() const = 0;
    virtual uint32_t getTotalHsThreads() const = 0;
    virtual uint32_t getTotalDsThreads() const = 0;
    virtual uint32_t getTotalGsThreads() const = 0;
    virtual uint32_t getTotalPsThreads() const = 0;
    virtual uint32_t getMaxFillRate() const = 0;
    virtual uint32_t getMaxRCS() const = 0;
    virtual uint32_t getMaxCCS() const = 0;
    virtual void checkSysInfoMismatch(HardwareInfo *hwInfo) = 0;
};

inline SystemInfo::~SystemInfo() {}

} // namespace NEO
