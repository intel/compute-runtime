/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_excludes.h"

HWTEST_EXCLUDE_PRODUCT(ExcludeTestPtl, givenHwCmdTestWhenPtlExcludedThenDontRunOnPtl, IGFX_PTL);
HWTEST_EXCLUDE_PRODUCT(ExcludeTestPtl, givenHwTestWhenPtlExcludedThenDontRunOnPtl, IGFX_PTL);
