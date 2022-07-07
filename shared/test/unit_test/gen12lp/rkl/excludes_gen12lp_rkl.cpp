/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfForceEmuInt32DivRemSPWAIsRequiredThenFalseIsReturned, IGFX_ROCKETLAKE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIf3DPipelineSelectWAIsRequiredThenFalseIsReturned, IGFX_ROCKETLAKE);
