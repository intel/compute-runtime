/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"

#include <cstdint>
#include <string>

#ifndef BIT
#define BIT(x) (1ull << (x))
#endif

namespace NEO {
struct CpuInfo {
    static const uint64_t featureNone = 0x000000000ULL;
    static const uint64_t featureAvX2 = 0x000800000ULL;
    static const uint64_t featureNeon = 0x001000000ULL;
    static const uint64_t featureClflush = 0x2000000000ULL;

    CpuInfo() : features(featureNone) {
    }

    void cpuid(
        uint32_t cpuInfo[4],
        uint32_t functionId) const;

    void cpuidex(
        uint32_t cpuInfo[4],
        uint32_t functionId,
        uint32_t subfunctionId) const;

    void detect() const;

    bool isFeatureSupported(uint64_t feature) const {
        if (features == featureNone) {
            detect();
        }

        return (features & feature) == feature;
    }

    uint32_t getVirtualAddressSize() const {
        if (features == featureNone) {
            detect();
        }

        return virtualAddressSize;
    }

    bool isCpuFlagPresent(const char *cpuFlag) const {
        if (cpuFlags.empty()) {
            getCpuFlagsFunc(cpuFlags);
        }

        return cpuFlags.find(cpuFlag) != std::string::npos;
    }

    static const CpuInfo &getInstance() {
        return instance;
    }

    static void (*cpuidexFunc)(int *, int, int);
    static void (*cpuidFunc)(int[4], int);
    static void (*getCpuFlagsFunc)(std::string &);

  protected:
    mutable uint64_t features;
    mutable uint32_t virtualAddressSize = is32bit ? 32 : 48;
    mutable std::string cpuFlags;
    static const CpuInfo instance;
};

} // namespace NEO
