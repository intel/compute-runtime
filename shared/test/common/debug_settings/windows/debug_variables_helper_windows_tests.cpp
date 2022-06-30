/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_variables_helper.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

TEST(DebugVariablesHelperTests, whenIsDebugKeysReadEnableIsCalledThenFalseIsReturned) {
    EXPECT_FALSE(NEO::isDebugKeysReadEnabled());
}

} // namespace NEO
