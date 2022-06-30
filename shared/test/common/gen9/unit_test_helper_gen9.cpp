/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/gen9/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_bdw_and_later.inl"

namespace NEO {
using Family = SKLFamily;

template <>
bool UnitTestHelper<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return true;
}

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterOffset() {
    return 0x20ec;
}

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterValue() {
    return (1u << 6) | (1u << 22);
}

template <>
uint32_t UnitTestHelper<Family>::getTdCtlRegisterOffset() {
    return 0xe400;
}

template <>
uint32_t UnitTestHelper<Family>::getTdCtlRegisterValue() {
    return (1u << 7) | (1u << 4);
}

template struct UnitTestHelper<Family>;
} // namespace NEO
