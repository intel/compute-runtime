/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(DebuggerSingleAddressSpaceAub, GivenSingleAddressSpaceWhenCmdListIsExecutedThenSbaAddressesAreTracked, IGFX_XE_HPG_CORE);

HWTEST_EXCLUDE_PRODUCT(DebuggerGlobalAllocatorAub, GivenKernelWithScratchWhenCmdListExecutedThenSbaAddressesAreTracked_PlatformsSupportingGlobalBindless, IGFX_XE_HPG_CORE);
} // namespace ult
} // namespace L0
