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
        memoryManager = new OsAgnosticMemoryManager(false, false);
        memoryManager->csr = createCommandStream(*platformDevices, executionEnvironment);
    }

    void TearDown() override {
        delete memoryManager->csr;
        delete memoryManager;
        MemoryManagementFixture::TearDown();
    }

  protected:
    ExecutionEnvironment executionEnvironment;
    MemoryManager *memoryManager;
};
