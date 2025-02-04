/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ExcludeTestPtl, givenHwCmdTestWhenPtlExcludedThenDontRunOnPtl, IGFX_PTL);
HWTEST_EXCLUDE_PRODUCT(ExcludeTestPtl, givenHwTestWhenPtlExcludedThenDontRunOnPtl, IGFX_PTL);
