/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

HWTEST_EXCLUDE_PRODUCT(DeviceFactoryTest, givenInvalidHwConfigStringWhenPrepareDeviceEnvironmentsForProductFamilyOverrideThenThrowsException, IGFX_ROCKETLAKE);
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn, IGFX_ROCKETLAKE)
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy, IGFX_ROCKETLAKE);
