/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/gen11/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_bdw_and_later.inl"

namespace NEO {

using Family = Gen11Family;

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterOffset() {
    return 0x20d8;
}

template struct UnitTestHelper<Gen11Family>;
} // namespace NEO
