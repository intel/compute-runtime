/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"

#include <memory>

namespace NEO {
class Device;
struct HardwareInfo;

namespace TestChecks {
bool supportsBlitter(const HardwareInfo *pHardwareInfo);
bool fullySupportsBlitter(const HardwareInfo *pHardwareInfo);
bool supportsImages(const HardwareInfo &hardwareInfo);
bool supportsImages(const std::unique_ptr<HardwareInfo> &pHardwareInfo);
bool supportsSvm(const HardwareInfo *pHardwareInfo);
bool supportsSvm(const std::unique_ptr<HardwareInfo> &pHardwareInfo);
bool supportsSvm(const Device *pDevice);
} // namespace TestChecks

} // namespace NEO

#define REQUIRE_32BIT_OR_SKIP()        \
    if constexpr (::is64bit == true) { \
        GTEST_SKIP();                  \
    }

#define REQUIRE_64BIT_OR_SKIP()         \
    if constexpr (::is64bit == false) { \
        GTEST_SKIP();                   \
    }

#define REQUIRE_SVM_OR_SKIP(param)                      \
    if (NEO::TestChecks::supportsSvm(param) == false) { \
        GTEST_SKIP();                                   \
    }

#define REQUIRE_BLITTER_OR_SKIP(param)                      \
    if (NEO::TestChecks::supportsBlitter(param) == false) { \
        GTEST_SKIP();                                       \
    }

#define REQUIRE_FULL_BLITTER_OR_SKIP(param)                      \
    if (NEO::TestChecks::fullySupportsBlitter(param) == false) { \
        GTEST_SKIP();                                            \
    }

#define REQUIRE_IMAGES_OR_SKIP(param)                      \
    if (NEO::TestChecks::supportsImages(param) == false) { \
        GTEST_SKIP();                                      \
    }
