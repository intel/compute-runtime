/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

struct MockTileDebugSessionLinux : TileDebugSessionLinux {
    using DebugSessionImp::stateSaveAreaHeader;
    using TileDebugSessionLinux::getContextStateSaveAreaGpuVa;
    using TileDebugSessionLinux::readGpuMemory;
    using TileDebugSessionLinux::readModuleDebugArea;
    using TileDebugSessionLinux::readSbaBuffer;
    using TileDebugSessionLinux::readStateSaveAreaHeader;
    using TileDebugSessionLinux::startAsyncThread;
    using TileDebugSessionLinux::TileDebugSessionLinux;
    using TileDebugSessionLinux::writeGpuMemory;
};

TEST(TileDebugSessionLinuxTest, GivenTileDebugSessionWhenCallingFunctionsThenUnsupportedErrorIsReturnedOrImplementationIsEmpty) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto session = std::make_unique<MockTileDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, nullptr);
    ASSERT_NE(nullptr, session);

    ze_device_thread_t thread = {0, 0, 0, 0};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->interrupt(thread));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->resume(thread));

    zet_debug_memory_space_desc_t desc = {};
    size_t size = 10;
    char buffer[10];

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readMemory(thread, &desc, size, buffer));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->writeMemory(thread, &desc, size, buffer));

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readGpuMemory(0, nullptr, 0, 0));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->writeGpuMemory(0, nullptr, 0, 0));

    zet_debug_event_t event = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->acknowledgeEvent(&event));

    uint32_t type = 0;
    uint32_t start = 0;
    uint32_t count = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readRegisters(thread, type, start, count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->writeRegisters(thread, type, start, count, nullptr));

    EuThread::ThreadId threadId{0, 0, 0, 0, 0};
    NEO::SbaTrackedAddresses sbaBuffer = {};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readSbaBuffer(threadId, sbaBuffer));

    EXPECT_TRUE(session->closeConnection());
    EXPECT_TRUE(session->readModuleDebugArea());

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->initialize());
    EXPECT_EQ(0u, session->getContextStateSaveAreaGpuVa(0));

    session->readStateSaveAreaHeader();
    EXPECT_TRUE(session->stateSaveAreaHeader.empty());

    EXPECT_THROW(session->startAsyncThread(), std::exception);
}

} // namespace ult
} // namespace L0
