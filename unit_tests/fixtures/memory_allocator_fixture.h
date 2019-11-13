/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_helper.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace NEO;

class MemoryAllocatorFixture : public MemoryManagementFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->setHwInfo(*platformDevices);
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0u));
        memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        csr = &device->getGpgpuCommandStreamReceiver();
        auto engineType = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0];
        auto osContext = memoryManager->createAndRegisterOsContext(csr, engineType, 1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
        csr->setupContext(*osContext);
    }

    void TearDown() override {
        device.reset();
        MemoryManagementFixture::TearDown();
    }

  protected:
    std::unique_ptr<MockDevice> device;
    ExecutionEnvironment *executionEnvironment;
    MockMemoryManager *memoryManager = nullptr;
    CommandStreamReceiver *csr = nullptr;
};
