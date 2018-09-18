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
    gmockMemoryManager = new NiceMock<GMockMemoryManager>;
    memoryManager = gmockMemoryManager;

    ON_CALL(*gmockMemoryManager, cleanAllocationList(::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(gmockMemoryManager, &GMockMemoryManager::MemoryManagerCleanAllocationList));
    ON_CALL(*gmockMemoryManager, populateOsHandles(::testing::_)).WillByDefault(::testing::Invoke(gmockMemoryManager, &GMockMemoryManager::MemoryManagerPopulateOsHandles));

    csr->tagAddress = &currentGpuTag;
    memoryManager->csr = csr.get();
}

void MemoryManagerWithCsrFixture::TearDown() {
    delete memoryManager;
}
