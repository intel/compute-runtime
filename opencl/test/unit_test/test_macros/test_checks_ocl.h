/*
 * Copyright (C) 2020-2022 Intel Corporation
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

namespace TestChecks {
bool supportsSvm(const ClDevice *pClDevice);
bool supportsImages(const Context *pContext);
bool supportsOcl21(const std::unique_ptr<HardwareInfo> &pHardwareInfo);
bool supportsPipes(const ClDevice *pClDevice);
bool supportsAuxResolves();
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

#define REQUIRE_AUX_RESOLVES()                             \
    if (NEO::TestChecks::supportsAuxResolves() == false) { \
        GTEST_SKIP();                                      \
    }
