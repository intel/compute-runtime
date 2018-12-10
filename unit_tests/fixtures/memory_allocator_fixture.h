/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/preemption.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/libult/create_command_stream.h"

using namespace OCLRT;

class MemoryAllocatorFixture : public MemoryManagementFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->initializeCommandStreamReceiver(*platformDevices, 0, 0);
        memoryManager = new OsAgnosticMemoryManager(false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        csr = memoryManager->getDefaultCommandStreamReceiver(0);
        csr->setOsContext(*memoryManager->createAndRegisterOsContext(gpgpuEngineInstances[0], PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0])));
    }

    void TearDown() override {
        executionEnvironment.reset();
        MemoryManagementFixture::TearDown();
    }

  protected:
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    MemoryManager *memoryManager;
    CommandStreamReceiver *csr;
};
