/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsSystolicModeConfigurabledThenFalseIsReturned, IGFX_ALDERLAKE_P);
HWTEST_EXCLUDE_PRODUCT(PreambleTest, WhenIsSystolicModeConfigurableThenReturnFalse, IGFX_ALDERLAKE_P);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfPipeControlWAIsRequiredThenFalseIsReturned, IGFX_ALDERLAKE_P);
