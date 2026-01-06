/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/fan/sysman_fan_imp.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockFanHandleContext : public FanHandleContext {

    uint32_t mockSupportedFanCount = 1;

    MockFanHandleContext(OsSysman *pOsSysman) : FanHandleContext(pOsSysman) {}

    void init() {
        bool multipleFansSupported = (mockSupportedFanCount > 1);
        for (uint32_t fanIndex = 0; fanIndex < mockSupportedFanCount; fanIndex++) {
            createHandle(fanIndex, multipleFansSupported);
        }
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
