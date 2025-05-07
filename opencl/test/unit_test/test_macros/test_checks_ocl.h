/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

namespace NEO {
class ClDevice;
class Context;
struct HardwareInfo;
struct RootDeviceEnvironment;

namespace TestChecks {
bool supportsImages(const Context *pContext);
bool supportsOcl21(const std::unique_ptr<HardwareInfo> &pHardwareInfo);
bool supportsAuxResolves(const RootDeviceEnvironment &rootDeviceEnvironment);
} // namespace TestChecks

} // namespace NEO

#include "shared/test/common/test_macros/test_checks_shared.h"

#define REQUIRE_IMAGE_SUPPORT_OR_SKIP(param)                                     \
    auto hwInfo = castToObject<Context>(param)->getDevice(0)->getHardwareInfo(); \
    if (!hwInfo.capabilityTable.supportsImages) {                                \
        GTEST_SKIP();                                                            \
    }

#define REQUIRE_OCL_21_OR_SKIP(param)                     \
    if (NEO::TestChecks::supportsOcl21(param) == false) { \
        GTEST_SKIP();                                     \
    }

#define REQUIRE_AUX_RESOLVES(rootDeviceEnvironment)                             \
    if (NEO::TestChecks::supportsAuxResolves(rootDeviceEnvironment) == false) { \
        GTEST_SKIP();                                                           \
    }
