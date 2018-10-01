/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/memory_manager_fixture.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace OCLRT;
using ::testing::NiceMock;

void MemoryManagerWithCsrFixture::SetUp() {
    csr = new MockCommandStreamReceiver(this->executionEnvironment);
    gmockMemoryManager = new NiceMock<GMockMemoryManager>(executionEnvironment);
    memoryManager = gmockMemoryManager;

    ON_CALL(*gmockMemoryManager, cleanAllocationList(::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(gmockMemoryManager, &GMockMemoryManager::MemoryManagerCleanAllocationList));
    ON_CALL(*gmockMemoryManager, populateOsHandles(::testing::_)).WillByDefault(::testing::Invoke(gmockMemoryManager, &GMockMemoryManager::MemoryManagerPopulateOsHandles));

    csr->tagAddress = &currentGpuTag;
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(csr));
}

void MemoryManagerWithCsrFixture::TearDown() {
    delete memoryManager;
}
