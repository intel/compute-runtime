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
    std::unique_ptr<L0::FenceImp> fence = std::make_unique<L0::FenceImp>(&cmdQueue);

    bool result = fence->initialize();
    ASSERT_TRUE(result);
    EXPECT_FALSE(csr->downloadAllocationsCalled);

    fence->queryStatus();
    EXPECT_TRUE(csr->downloadAllocationsCalled);
}

} // namespace ult
} // namespace L0
