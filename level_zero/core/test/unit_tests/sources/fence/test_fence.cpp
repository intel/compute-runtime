/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_command_stream_receiver.h"

#include "test.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using FenceTest = Test<DeviceFixture>;
TEST_F(FenceTest, whenQueryingStatusThenCsrAllocationsAreDownloaded) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0);

    Mock<CommandQueue> cmdQueue(device, csr.get());
    auto fence = Fence::create(&cmdQueue, nullptr);

    EXPECT_NE(nullptr, fence);

    auto &graphicsAllocation = fence->getAllocation();

    EXPECT_EQ(neoDevice->getRootDeviceIndex(), graphicsAllocation.getRootDeviceIndex());

    EXPECT_FALSE(csr->downloadAllocationsCalled);

    auto status = fence->queryStatus();
    EXPECT_EQ(ZE_RESULT_NOT_READY, status);
    EXPECT_TRUE(csr->downloadAllocationsCalled);

    status = fence->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, status);
}

} // namespace ult
} // namespace L0
