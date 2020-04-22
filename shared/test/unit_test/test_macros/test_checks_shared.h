/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gtest/gtest.h"

#include <memory>

namespace NEO {
class Device;
struct HardwareInfo;

namespace TestChecks {
bool supportsSvm(const HardwareInfo *pHardwareInfo);
bool supportsSvm(const std::unique_ptr<HardwareInfo> &pHardwareInfo);
bool supportsSvm(const Device *pDevice);
} // namespace TestChecks

} // namespace NEO

#define REQUIRE_SVM_OR_SKIP(param)                      \
    if (NEO::TestChecks::supportsSvm(param) == false) { \
        GTEST_SKIP();                                   \
    }
