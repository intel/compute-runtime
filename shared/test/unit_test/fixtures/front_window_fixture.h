/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/cache_policy.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

namespace NEO {

class CommandContainer;

class MemManagerFixture : public DeviceFixture {
  public:
    struct FrontWindowMemManagerMock : public MockMemoryManager {
        FrontWindowMemManagerMock(NEO::ExecutionEnvironment &executionEnvironment);
        void forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range);
    };

    void setUp();
    void tearDown();

    std::unique_ptr<FrontWindowMemManagerMock> memManager;
};

struct BindlessCommandEncodeStatesFixture : public MemManagerFixture {
    void setUp();
    void tearDown();

    ~BindlessCommandEncodeStatesFixture();

    NEO::L1CachePolicy l1CachePolicyData;
    std::unique_ptr<CommandContainer> cmdContainer;
};

} // namespace NEO
