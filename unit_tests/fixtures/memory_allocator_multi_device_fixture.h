/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace NEO;

template <uint32_t numRootDevices>
class MemoryAllocatorMultiDeviceFixture : public MemoryManagementFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->setHwInfo(*platformDevices);
        executionEnvironment->rootDeviceEnvironments.resize(numRootDevices);
        memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
    }

    void TearDown() override {
        executionEnvironment.reset();
        MemoryManagementFixture::TearDown();
    }

    uint32_t getNumRootDevices() { return numRootDevices; }

  protected:
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    MockMemoryManager *memoryManager = nullptr;
};
