/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfAdditionalMediaSamplerProgrammingIsRequiredThenFalseIsReturned, IGFX_ICELAKE_LP)
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfInitialFlagsProgrammingIsRequiredThenFalseIsReturned, IGFX_ICELAKE_LP)
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfReturnedCmdSizeForMediaSamplerAdjustmentIsRequiredThenFalseIsReturned, IGFX_ICELAKE_LP)
