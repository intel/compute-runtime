/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

#ifndef BIT
#define BIT(x) (1ull << (x))
#endif

namespace OCLRT {
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

    CpuInfo() : features(featureNone) {
    }

    void cpuid(
        uint32_t cpuInfo[4],
        uint32_t functionId) const;

    void cpuidex(
        uint32_t cpuInfo[4],
        uint32_t functionId,
        uint32_t subfunctionId) const;

    void detect() const {
        uint32_t cpuInfo[4];

        cpuid(cpuInfo, 0u);
        auto numFunctionIds = cpuInfo[0];

        if (numFunctionIds >= 1u) {
            cpuid(cpuInfo, 1u);
            {
                features |= cpuInfo[3] & BIT(0) ? featureFpu : featureNone;
            }

            {
                features |= cpuInfo[3] & BIT(15) ? featureCmov : featureNone;
            }

            {
                features |= cpuInfo[3] & BIT(23) ? featureMmx : featureNone;
            }

            {
                features |= cpuInfo[3] & BIT(24) ? featureFxsave : featureNone;
            }

            {
                features |= cpuInfo[3] & BIT(25) ? featureSse : featureNone;
            }

            {
                features |= cpuInfo[3] & BIT(26) ? featureSsE2 : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(0) ? featureSsE3 : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(1) ? featurePclmulqdq : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(9) ? featureSssE3 : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(12) ? featureFma : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(19) ? featureSsE41 : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(20) ? featureSsE42 : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(22) ? featureMovbe : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(23) ? featurePopcnt : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(25) ? featureAes : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(28) ? featureAvx : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(29) ? featureF16C : featureNone;
            }

            {
                features |= cpuInfo[2] & BIT(30) ? featureRdrnd : featureNone;
            }
        }

        if (numFunctionIds >= 7u) {
            cpuid(cpuInfo, 7u);
            {
                auto mask = BIT(5) | BIT(3) | BIT(8);
                features |= (cpuInfo[1] & mask) == mask ? featureAvX2 : featureNone;
            }

            {
                auto mask = BIT(3) | BIT(8);
                features |= (cpuInfo[1] & mask) == mask ? featureBmi : featureNone;
            }

            {
                features |= cpuInfo[1] & BIT(4) ? featureHle : featureNone;
            }

            {
                features |= cpuInfo[1] & BIT(11) ? featureRtm : featureNone;
            }
        }

        cpuid(cpuInfo, 0x80000000);
        auto maxExtendedId = cpuInfo[0];
        if (maxExtendedId >= 0x80000001) {
            cpuid(cpuInfo, 0x80000001);
            {
                features |= cpuInfo[2] & BIT(5) ? featureLzcnt : featureNone;
            }
        }
    }

    bool isFeatureSupported(uint64_t feature) const {
        if (features == featureNone) {
            detect();
        }

        return (features & feature) == feature;
    }

    static const CpuInfo &getInstance() {
        return instance;
    }

    static void (*cpuidexFunc)(int *, int, int);

  protected:
    mutable uint64_t features;
    static const CpuInfo instance;
};

} // namespace OCLRT
