/*
 * Copyright (C) 2020-2025 Intel Corporation
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
struct RootDeviceEnvironment;

namespace TestChecks {
bool supportsBlitter(const RootDeviceEnvironment &rootDeviceEnvironment);
bool fullySupportsBlitter(const RootDeviceEnvironment &rootDeviceEnvironment);
bool allowsDcFlush(const Device *device);
bool supportsImages(const HardwareInfo &hardwareInfo);
bool supportsImages(const std::unique_ptr<HardwareInfo> &pHardwareInfo);
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

#define REQUIRE_BLITTER_OR_SKIP(param)                      \
    if (NEO::TestChecks::supportsBlitter(param) == false) { \
        GTEST_SKIP();                                       \
    }

#define REQUIRE_DC_FLUSH_OR_SKIP(param)                   \
    if (NEO::TestChecks::allowsDcFlush(param) == false) { \
        GTEST_SKIP();                                     \
    }

#define REQUIRE_FULL_BLITTER_OR_SKIP(param)                      \
    if (NEO::TestChecks::fullySupportsBlitter(param) == false) { \
        GTEST_SKIP();                                            \
    }

#define REQUIRE_IMAGES_OR_SKIP(param)                      \
    if (NEO::TestChecks::supportsImages(param) == false) { \
        GTEST_SKIP();                                      \
    }
