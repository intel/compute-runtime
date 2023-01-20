/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfPipeControlWAIsRequiredThenFalseIsReturned, IGFX_ALDERLAKE_N);
