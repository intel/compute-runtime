/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(HwHelperTest, GivenZeroSlmSizeWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTestXeHpAndLater, givenXeHPAndLaterPlatformWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenFalseIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(WddmMemoryManagerSimpleTest, givenLinearStreamWhenItIsAllocatedThenItIsInLocalMemoryHasCpuPointerAndHasStandardHeap64kbAsGpuAddress, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(HwHelperTestXeHPAndLater, GiveCcsNodeThenDefaultEngineTypeIsCcs, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterDeviceCapsTests, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue, IGFX_DG2);
