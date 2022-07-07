/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfPipeControlWAIsRequiredThenFalseIsReturned, IGFX_DG1);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfImagePitchAlignmentWAIsRequiredThenFalseIsReturned, IGFX_DG1);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfForceEmuInt32DivRemSPWAIsRequiredThenFalseIsReturned, IGFX_DG1);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIf3DPipelineSelectWAIsRequiredThenFalseIsReturned, IGFX_DG1);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfStorageInfoAdjustmentIsRequiredThenFalseIsReturned, IGFX_DG1);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, whenOverrideGfxPartitionLayoutForWslThenReturnFalse, IGFX_DG1);
