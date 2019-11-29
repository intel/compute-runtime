/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_info.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/helpers/unit_test_helper.inl"

namespace NEO {
using Family = SKLFamily;

template <>
bool UnitTestHelper<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return true;
}

template struct UnitTestHelper<Family>;
} // namespace NEO
