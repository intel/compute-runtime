/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/utilities/hw_timestamps.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(CsrDepsTests, givenCsrDepsWhenMakeResidentCalledThenMultiRootSyncNodeAreMadeResident) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.memoryManager = std::make_unique<MockMemoryManager>();
    MockCommandStreamReceiver csr(executionEnvironment, 0, 0);
    CsrDependencies csrDeps;
    MockTagAllocator<HwTimeStamps> timeStampAllocator(0u, executionEnvironment.memoryManager.get(), 10,
                                                      MemoryConstants::cacheLineSize, sizeof(HwTimeStamps), false, 0);
    auto timestampPacketContainer = std::make_unique<TimestampPacketContainer>();

    auto node = timeStampAllocator.getTag();
    timestampPacketContainer->add(node);

    csrDeps.multiRootTimeStampSyncContainer.push_back(timestampPacketContainer.get());
    csrDeps.makeResident(csr);
    EXPECT_EQ(csr.makeResidentCalledTimes, 1u);
}