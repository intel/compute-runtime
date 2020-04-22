/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
class ClDevice;
class Context;

namespace TestChecks {
bool supportsSvm(const ClDevice *pClDevice);
bool supportsImages(const Context *pContext);
bool supportsOcl21(const Context *pContext);
} // namespace TestChecks

} // namespace NEO

#include "shared/test/unit_test/test_macros/test_checks_shared.h"

#define REQUIRE_IMAGES_OR_SKIP(param)                      \
    if (NEO::TestChecks::supportsImages(param) == false) { \
        GTEST_SKIP();                                      \
    }

#define REQUIRE_OCL_21_OR_SKIP(param)                     \
    if (NEO::TestChecks::supportsOcl21(param) == false) { \
        GTEST_SKIP();                                     \
    }
