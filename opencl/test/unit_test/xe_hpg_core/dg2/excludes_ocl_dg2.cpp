/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProgramTests, givenAtLeastXeHpgCoreWhenGetInternalOptionsThenCorrectBuildOptionIsSet_IsAtLeastXeCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, givenL3ToL1DebugFlagWhenStatelessMocsIsProgrammedThenItHasL1CachingOn, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferConstPolicy, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(CmdsProgrammingTestsXeHpgCore, whenAppendingRssThenProgramWBPL1CachePolicy, IGFX_DG2);
