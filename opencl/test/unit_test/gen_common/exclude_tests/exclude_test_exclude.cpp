/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

HWTEST_EXCLUDE_PRODUCT(ExcludeTestBdw, givenHwCmdTestWhenBdwExcludedThenDontRunOnBdw, IGFX_BROADWELL);
HWTEST_EXCLUDE_PRODUCT(ExcludeTestBdw, givenHwTestWhenBdwExcludedThenDontRunOnBdw, IGFX_BROADWELL);
HWTEST_EXCLUDE_PRODUCT(ExcludeTestSkl, givenHwCmdTestWhenSklExcludedThenDontRunOnSkl, IGFX_SKYLAKE);
HWTEST_EXCLUDE_PRODUCT(ExcludeTestSkl, givenHwTestWhenSklExcludedThenDontRunOnSkl, IGFX_SKYLAKE);
