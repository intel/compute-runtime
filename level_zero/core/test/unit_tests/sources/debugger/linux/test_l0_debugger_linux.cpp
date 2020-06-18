/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

using L0DebuggerLinuxTest = Test<L0DebuggerFixture>;

TEST_F(L0DebuggerLinuxTest, givenProgramDebuggingEnabledWhenDriverHandleIsCreatedThenItAllocatesL0Debugger) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());

    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

} // namespace ult
} // namespace L0
