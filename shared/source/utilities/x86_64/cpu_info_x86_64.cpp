/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#ifndef BIT
#define BIT(x) (1ull << (x))
#endif

namespace NEO {
void CpuInfo::detect() const {
    uint32_t cpuInfo[4] = {};

    cpuid(cpuInfo, 0u);
    auto numFunctionIds = cpuInfo[0];

    if (numFunctionIds >= 1u) {
        cpuid(cpuInfo, 1u);
        {
            features |= cpuInfo[3] & BIT(0) ? featureFpu : featureNone;
        }

        {
            features |= cpuInfo[3] & BIT(4) ? featureTsc : featureNone;
        }

        {
            features |= cpuInfo[3] & BIT(15) ? featureCmov : featureNone;
        }

        {
            features |= cpuInfo[3] & BIT(19) ? featureClflush : featureNone;
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

        {
            features |= cpuInfo[3] & BIT(27) ? featureRdtscp : featureNone;
        }
    }

    if (maxExtendedId >= 0x80000008) {
        cpuid(cpuInfo, 0x80000008);
        {
            virtualAddressSize = (cpuInfo[0] >> 8) & 0xFF;
        }
    }
}
} // namespace NEO
