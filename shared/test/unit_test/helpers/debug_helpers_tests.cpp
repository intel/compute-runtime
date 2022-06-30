/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(debugBreak, whenDebugBreakCalledInTestThenNothingIsThrown) {
    DEBUG_BREAK_IF(!false);
}
