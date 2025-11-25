/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"
using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToLocalBufferStatelessIsUsedThenParamsAreCorrect, criProductEnumValue);
HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToSystemBufferStatelessIsUsedThenParamsAreCorrect, criProductEnumValue);
HWTEST_EXCLUDE_PRODUCT(CommandQueuePvcAndLaterTests, givenSplitBcsMaskWhenConstructBcsEnginesForSplitThenContainsGivenBcsEngines_IsAtLeastXeHpcCore, criProductEnumValue);
