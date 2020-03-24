/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_helper.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

using namespace NEO;

class MemoryAllocatorFixture : public MemoryManagementFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
        memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        csr = &device->getGpgpuCommandStreamReceiver();
        auto &hwInfo = device->getHardwareInfo();
        auto engineType = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0];
        auto osContext = memoryManager->createAndRegisterOsContext(csr, engineType, 1,
                                                                   PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo),
                                                                   false, false, false);
        csr->setupContext(*osContext);
    }

    void TearDown() override {
        device.reset();
        platformsImpl.clear();
        MemoryManagementFixture::TearDown();
    }

  protected:
    std::unique_ptr<MockDevice> device;
    ExecutionEnvironment *executionEnvironment;
    MockMemoryManager *memoryManager = nullptr;
    CommandStreamReceiver *csr = nullptr;
};
