/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwHelperTest, WhenAllowRenderCompressionIsCalledThenTrueIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwHelperTest, givenHwHelperWhenIsBlitterForImagesSupportedIsCalledThenFalseIsReturned, IGFX_XE_HP_CORE);
