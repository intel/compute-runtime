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

struct MockDebugSessionSipRegisterAccess : public MockDebugSession {
    MockDebugSessionSipRegisterAccess(const zet_debug_config_t &config, L0::Device *device)
        : MockDebugSession(config, device) {}

    bool callGetRegisterAccessProperties(EuThread::ThreadId *threadId, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties) {
        // Directly invoke the base implementation (stub) to exercise code path.
        return DebugSessionImp::getRegisterAccessProperties(threadId, pCount, pRegisterSetProperties);
    }
};

TEST_F(DebugApiLinuxSipTest, GivenStubImplementationWhenGetRegisterAccessPropertiesIsCalledThenReturnsTrueAndLeavesDataUnchanged) {
    zet_debug_config_t config = {};
    config.pid = 0x5678;
    auto session = std::make_unique<MockDebugSessionSipRegisterAccess>(config, device);
    ASSERT_NE(nullptr, session);

    // Prepare properties array with sentinel values to verify they are not modified by stub.
    zet_debug_regset_properties_t props[2] = {};
    props[0].type = ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU;
    props[0].count = 123u; // sentinel
    props[1].type = ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU;
    props[1].count = 456u; // sentinel
    uint32_t count = 2u;   // number of entries in props

    // Call with nullptr thread id (stub does not use it) to keep setup minimal.
    bool result = session->callGetRegisterAccessProperties(nullptr, &count, props);

    EXPECT_TRUE(result);
    // Verify sentinel values remain unchanged.
    EXPECT_EQ(123u, props[0].count);
    EXPECT_EQ(456u, props[1].count);
    // Count should also remain unchanged.
    EXPECT_EQ(2u, count);
}

struct MockDebugSessionGetThreadSipCounter : public MockDebugSession {
    MockDebugSessionGetThreadSipCounter(const zet_debug_config_t &config, L0::Device *device)
        : MockDebugSession(config, device) {}

    bool callGetThreadSipCounter(const void *stateSaveArea, L0::EuThread *thread, const NEO::StateSaveAreaHeader *stateSaveAreaHeader, uint64_t *sipThreadCounter) {
        // Directly invoke the base implementation to test the stub.
        return DebugSessionImp::getThreadSipCounter(stateSaveArea, thread, stateSaveAreaHeader, sipThreadCounter);
    }
};

TEST_F(DebugApiLinuxSipTest, GivenGetThreadSipCounterImplementationWhenGetThreadSipCounterIsCalledThenReturnsFalse) {
    zet_debug_config_t config = {};
    config.pid = 0x9999;
    auto session = std::make_unique<MockDebugSessionGetThreadSipCounter>(config, device);
    ASSERT_NE(nullptr, session);

    uint64_t sipThreadCounter = 0xDEADBEEF; // sentinel value
    bool result = session->callGetThreadSipCounter(nullptr, nullptr, nullptr, &sipThreadCounter);

    EXPECT_FALSE(result);
    // Verify that sipThreadCounter was not modified by the stub
    EXPECT_EQ(0xDEADBEEF, sipThreadCounter);
}

} // namespace ult
} // namespace L0
