/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/gen8/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_bdw_and_later.inl"

namespace NEO {

using Family = Gen8Family;

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterOffset() {
    return 0x20ec;
}

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterValue() {
    return (1u << 6) | (1u << 22);
}

template struct UnitTestHelper<Gen8Family>;
} // namespace NEO
