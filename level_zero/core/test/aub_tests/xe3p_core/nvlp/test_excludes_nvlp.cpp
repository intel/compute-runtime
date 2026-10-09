/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(AUBVariableRegisterPerThreadL0, givenZeOptRegisterFileSizeOptionWhenExecutingKernelThenCorrectValuesAreReturned_IsAtLeastXe3Core, IGFX_NVL);
}
} // namespace L0
