/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(DebuggerSingleAddressSpaceAub, GivenSingleAddressSpaceWhenCmdListIsExecutedThenSbaAddressesAreTracked_PlatformsSupportingSingleAddressSpace, IGFX_XE_HPG_CORE);
}
} // namespace L0
