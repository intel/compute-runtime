/*
 * Copyright (C) 2024-2025 Intel Corporation
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

struct MockDebugSessionSip : public MockDebugSession {

    MockDebugSessionSip(const zet_debug_config_t &config, L0::Device *device)
        : MockDebugSession(config, device) {}

    bool testOpenSipWrapper(uint64_t contextHandle, uint64_t gpuVa) {
        NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(connectedDevice->getNEODevice());
        return openSipWrapper(neoDevice, contextHandle, gpuVa);
    }

    bool testCloseSipWrapper(uint64_t contextHandle) {
        NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(connectedDevice->getNEODevice());
        return closeSipWrapper(neoDevice, contextHandle);
    }
};

TEST_F(DebugApiLinuxSipTest, GivenValidDeviceWhenOpenSipWrapperIsCalledThenFunctionExecutesSuccessfully) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto session = std::make_unique<MockDebugSessionSip>(config, device);
    ASSERT_NE(nullptr, session);

    EXPECT_TRUE(session->testOpenSipWrapper(0x1111, 0x2222));
}

TEST_F(DebugApiLinuxSipTest, GivenValidDeviceWhenCloseSipWrapperIsCalledThenFunctionExecutesSuccessfully) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto session = std::make_unique<MockDebugSessionSip>(config, device);
    ASSERT_NE(nullptr, session);

    EXPECT_TRUE(session->testCloseSipWrapper(0x3333));
}

} // namespace ult
} // namespace L0
