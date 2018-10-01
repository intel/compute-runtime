/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/libult/create_command_stream.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/execution_environment/execution_environment.h"

using namespace OCLRT;

class MemoryAllocatorFixture : public MemoryManagementFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->initializeCommandStreamReceiver(*platformDevices, 0u);
        memoryManager = new OsAgnosticMemoryManager(false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
    }

    void TearDown() override {
        executionEnvironment.reset();
        MemoryManagementFixture::TearDown();
    }

  protected:
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    MemoryManager *memoryManager;
};
