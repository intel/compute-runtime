/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestBdw, givenHwCmdTestWhenBdwExcludedDontRunOnBdw, IGFX_BROADWELL);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestBdw, givenHwTestWhenBdwExcludedDontRunOnBdw, IGFX_BROADWELL);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestSkl, givenHwCmdTestWhenSklExcludedDontRunOnSkl, IGFX_SKYLAKE);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTestSkl, givenHwTestWhenSklExcludedDontRunOnSkl, IGFX_SKYLAKE);
