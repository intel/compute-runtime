/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledAndStateSipWhenItIsRequiredThenThereIsPipeControlPriorToIt, IGFX_XE_HP_CORE);
