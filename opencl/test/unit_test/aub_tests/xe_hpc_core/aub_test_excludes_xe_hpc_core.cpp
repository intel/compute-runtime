/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

HWTEST_EXCLUDE_PRODUCT(MiAtomicAubTest, GivenSystemMemoryWhenDispatchingAtomicMove4BytesOperationThenExpectCorrectEndValues, IGFX_XE_HPC_CORE);
