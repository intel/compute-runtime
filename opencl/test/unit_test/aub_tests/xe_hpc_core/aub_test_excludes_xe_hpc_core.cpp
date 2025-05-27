/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(MiAtomicAubTest, GivenSystemMemoryWhenDispatchingAtomicMove4BytesOperationThenExpectCorrectEndValues, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(SingleTileDualContextTest, givenSingleAllocationWhenUpdatedFromDifferentContextThenDataIsValid_IsPVC, IGFX_XE_HPC_CORE);
