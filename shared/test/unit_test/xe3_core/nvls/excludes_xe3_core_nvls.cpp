/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned, IGFX_NVL_XE3G);
