/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/debug_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(debugBreak, whenDebugBreakCalledInTestThenNothingIsThrown) {
    DEBUG_BREAK_IF(!false);
}
