/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestBdw, givenHwCmdTestWhenBdwExcludedThenDontRunOnBdw, IGFX_BROADWELL);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestBdw, givenHwTestWhenBdwExcludedThenDontRunOnBdw, IGFX_BROADWELL);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestSkl, givenHwCmdTestWhenSklExcludedThenDontRunOnSkl, IGFX_SKYLAKE);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestSkl, givenHwTestWhenSklExcludedThenDontRunOnSkl, IGFX_SKYLAKE);
