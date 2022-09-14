/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenIsSystolicModeConfigurabledThenFalseIsReturned, IGFX_ALDERLAKE_P);
HWTEST_EXCLUDE_PRODUCT(PreambleTest, WhenIsSystolicModeConfigurableThenReturnFalse, IGFX_ALDERLAKE_P);
