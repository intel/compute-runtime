/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"
using namespace NEO;

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(MultiTileCommandListSignalAllocLayoutTest, givenDynamicLayoutEnabledWhenAppendEventForProfilingCalledThenProgramOffsetMmio_IsAtLeastXeCore, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(L0DebuggerGlobalStatelessTest, givenGlobalStatelessWhenCmdListExecutedOnQueueThenQueueDispatchesSurfaceStateOnceToGlobalStatelessHeap_IsAtLeastXeCore, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(CommandQueueExecuteCommandListSWTagsTests, givenEnableSWTagsAndCommandListWithDifferentPreemtpionWhenExecutingCommandListThenPipeControlReasonTagIsInserted, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(MultiTileSynchronizedDispatchTests, givenLimitedSyncDispatchWhenAppendingThenProgramTokenCheck, xe3pCoreEnumValue);

} // namespace ult
} // namespace L0
