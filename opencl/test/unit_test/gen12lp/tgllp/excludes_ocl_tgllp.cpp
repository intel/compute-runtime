/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn, IGFX_TIGERLAKE_LP)
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy, IGFX_TIGERLAKE_LP);
