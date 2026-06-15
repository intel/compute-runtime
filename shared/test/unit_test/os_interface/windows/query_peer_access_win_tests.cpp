/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/query_peer_access.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

#include <limits>

using namespace NEO;

TEST(QueryPeerAccessWddmTest, givenWddmDriverModelWhenQueryingPeerAccessThenReturnsFalseWithoutProbing) {
    constexpr uint32_t numRootDevices = 2u;
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
    auto device0 = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
    auto device1 = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 1));

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(NEO::queryPeerAccess(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(nullptr, probeAllocation);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), handle);
}
