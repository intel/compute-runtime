/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

using DebugApiLinuxSipTest = Test<DebugApiFixture>;

TEST_F(DebugApiLinuxSipTest, GivenGetThreadSipCounterWithMemHandleImplementationWhenItIsCalledThenReturnsFalseAndCounterIsNotModified) {
    zet_debug_config_t config = {};
    config.pid = 0x9999;
    auto session = std::make_unique<MockDebugSession>(config, device);

    uint64_t sipThreadCounter = 0xDEADBEEF;
    EXPECT_FALSE(session->getThreadSipCounterWithMemHandle(nullptr, nullptr, nullptr, &sipThreadCounter, 0));
    EXPECT_EQ(0xDEADBEEF, sipThreadCounter);
}

} // namespace ult
} // namespace L0
