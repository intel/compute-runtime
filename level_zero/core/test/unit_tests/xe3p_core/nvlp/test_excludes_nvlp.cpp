/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(CommandListTestsReserveSize, givenCommandListWhenGetReserveSshSizeThen16slotSpaceReturned_IsHeapfulRequiredAndAtLeastXeCore, IGFX_NVL);
HWTEST_EXCLUDE_PRODUCT(CommandQueueGroupTest, givenVaryingBlitterSupportAndCCSThenBCSGroupContainsCorrectNumberOfEngines, IGFX_NVL);
HWTEST_EXCLUDE_PRODUCT(CommandQueueGroupTest, givenBlitterSupportAndCCSThenThreeQueueGroupsAreReturned, IGFX_NVL);
HWTEST_EXCLUDE_PRODUCT(ModuleTranslationUnitTest, givenAtLeastXeHpgCoreWhenGetInternalOptionsThenCorrectBuildOptionIsSet_IsAtLeastXe3pCore, IGFX_NVL);
} // namespace ult
} // namespace L0
