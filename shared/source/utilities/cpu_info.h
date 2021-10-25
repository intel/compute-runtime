/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    static const uint64_t featureGenericIA32 = 0x000000001ULL;
    static const uint64_t featureFpu = 0x000000002ULL;
    static const uint64_t featureCmov = 0x000000004ULL;
    static const uint64_t featureMmx = 0x000000008ULL;
    static const uint64_t featureFxsave = 0x000000010ULL;
    static const uint64_t featureSse = 0x000000020ULL;
    static const uint64_t featureSsE2 = 0x000000040ULL;
    static const uint64_t featureSsE3 = 0x000000080ULL;
    static const uint64_t featureSssE3 = 0x000000100ULL;
    static const uint64_t featureSsE41 = 0x000000200ULL;
    static const uint64_t featureSsE42 = 0x000000400ULL;
    static const uint64_t featureMovbe = 0x000000800ULL;
    static const uint64_t featurePopcnt = 0x000001000ULL;
    static const uint64_t featurePclmulqdq = 0x000002000ULL;
    static const uint64_t featureAes = 0x000004000ULL;
    static const uint64_t featureF16C = 0x000008000ULL;
    static const uint64_t featureAvx = 0x000010000ULL;
    static const uint64_t featureRdrnd = 0x000020000ULL;
    static const uint64_t featureFma = 0x000040000ULL;
    static const uint64_t featureBmi = 0x000080000ULL;
    static const uint64_t featureLzcnt = 0x000100000ULL;
    static const uint64_t featureHle = 0x000200000ULL;
    static const uint64_t featureRtm = 0x000400000ULL;
    static const uint64_t featureAvX2 = 0x000800000ULL;
    static const uint64_t featureKncni = 0x004000000ULL;
    static const uint64_t featureAvX512F = 0x008000000ULL;
    static const uint64_t featureAdx = 0x010000000ULL;
    static const uint64_t featureRdseed = 0x020000000ULL;
    static const uint64_t featureAvX512Er = 0x100000000ULL;
    static const uint64_t featureAvX512Pf = 0x200000000ULL;
    static const uint64_t featureAvX512Cd = 0x400000000ULL;
    static const uint64_t featureSha = 0x800000000ULL;
    static const uint64_t featureMpx = 0x1000000000ULL;
    static const uint64_t featureClflush = 0x2000000000ULL;
    static const uint64_t featureTsc = 0x4000000000ULL;
    static const uint64_t featureRdtscp = 0x8000000000ULL;

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
