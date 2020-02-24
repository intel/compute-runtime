/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_info.h"

#include "opencl/test/unit_test/helpers/unit_test_helper.h"
#include "opencl/test/unit_test/helpers/unit_test_helper.inl"

namespace NEO {
using Family = SKLFamily;

template <>
bool UnitTestHelper<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return true;
}

template struct UnitTestHelper<Family>;
} // namespace NEO
