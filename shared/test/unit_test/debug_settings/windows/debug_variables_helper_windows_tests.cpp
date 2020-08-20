/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_variables_helper.h"

#include "test.h"

namespace NEO {

TEST(DebugVariablesHelperTests, whenIsDebugKeysReadEnableIsCalledThenFalseIsReturned) {
    EXPECT_FALSE(NEO::isDebugKeysReadEnabled());
}

} // namespace NEO
