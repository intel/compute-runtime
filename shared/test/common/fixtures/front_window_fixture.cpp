/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/front_window_fixture.h"

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

MemManagerFixture::FrontWindowMemManagerMock::FrontWindowMemManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
void MemManagerFixture::FrontWindowMemManagerMock::forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range) { getGfxPartition(rootDeviceIndex)->init(range, 0, 0, gfxPartitions.size(), true); }
void MemManagerFixture::SetUp() {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    DeviceFixture::SetUp();
    memManager = std::unique_ptr<FrontWindowMemManagerMock>(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
}
void MemManagerFixture::TearDown() {
    DeviceFixture::TearDown();
}