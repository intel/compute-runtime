/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/debug_helpers.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(debugBreak, whenDebugBreakCalledInTestThenNothingIsThrown) {
    DEBUG_BREAK_IF(!false);
}
