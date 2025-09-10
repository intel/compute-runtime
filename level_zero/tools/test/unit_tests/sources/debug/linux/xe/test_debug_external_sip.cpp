/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/test/unit_tests/sources/debug/linux/xe/debug_session_fixtures_linux_xe.h"

namespace L0 {
namespace ult {

using DebugApiLinuxTestXe = Test<DebugApiLinuxXeFixture>;

struct MockDebugSessionLinuxXeMasterSip : public MockDebugSessionLinuxXe {

    MockDebugSessionLinuxXeMasterSip(const zet_debug_config_t &config, L0::Device *device, int debugFd)
        : MockDebugSessionLinuxXe(config, device, debugFd) {}

    bool testOpenSipWrapper(uint64_t contextHandle, uint64_t gpuVa) {
        NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(connectedDevice->getNEODevice());
        return openSipWrapper(neoDevice, contextHandle, gpuVa);
    }

    bool testCloseSipWrapper(uint64_t contextHandle) {
        NEO::MockDevice *neoDevice = static_cast<NEO::MockDevice *>(connectedDevice->getNEODevice());
        return closeSipWrapper(neoDevice, contextHandle);
    }
};

TEST_F(DebugApiLinuxTestXe, GivenValidDeviceWhenOpenSipWrapperIsCalledThenFunctionExecutesSuccessfully) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto session = std::make_unique<MockDebugSessionLinuxXeMasterSip>(config, device, 10);
    ASSERT_NE(nullptr, session);

    EXPECT_TRUE(session->testOpenSipWrapper(0x1111, 0x2222));
}

TEST_F(DebugApiLinuxTestXe, GivenValidDeviceWhenCloseSipWrapperIsCalledThenFunctionExecutesSuccessfully) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    auto session = std::make_unique<MockDebugSessionLinuxXeMasterSip>(config, device, 10);
    ASSERT_NE(nullptr, session);

    EXPECT_TRUE(session->testCloseSipWrapper(0x3333));
}

} // namespace ult
} // namespace L0
