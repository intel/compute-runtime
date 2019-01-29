/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

HWCMDTEST_EXCLUDE_FAMILY(ExcludeTest, whenBdwExcludedDontRunOnBdw, IGFX_BROADWELL);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTest, whenSklExcludedDontRunOnSkl, IGFX_SKYLAKE);
HWCMDTEST_EXCLUDE_FAMILY(ExcludeTest, whenCnlExcludedDontRunOnCnl, IGFX_CANNONLAKE);
