/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned, IGFX_LUNARLAKE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenCheckBlitEnqueuePreferredThenReturnTrue, IGFX_LUNARLAKE);
