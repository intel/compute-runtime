/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(AUBVariableRegisterPerThreadL0Xe3p, givenZeOptRegisterFileSizeOptionWhenExecutingKernelThenCorrectValuesAreReturned, IGFX_NVL);
}
} // namespace L0
