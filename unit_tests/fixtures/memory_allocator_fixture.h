/*
 * Copyright (C) 2017-2019 Intel Corporation
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
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace OCLRT;

class MemoryAllocatorFixture : public MemoryManagementFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->initializeCommandStreamReceiver(*platformDevices, 0, 0);
        memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        csr = memoryManager->getDefaultCommandStreamReceiver(0);
        csr->setupContext(*memoryManager->createAndRegisterOsContext(HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0])));
    }

    void TearDown() override {
        executionEnvironment.reset();
        MemoryManagementFixture::TearDown();
    }

  protected:
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    MockMemoryManager *memoryManager;
    CommandStreamReceiver *csr;
};
