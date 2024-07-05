/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(DeviceGetCapsTest, givenEnabledFtrPooledEuWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin, IGFX_XE_HPG_CORE);
