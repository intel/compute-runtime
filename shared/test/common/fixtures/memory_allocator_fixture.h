/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

using namespace NEO;

class MemoryAllocatorFixture : public MemoryManagementFixture {
  public:
    void setUp() {
        MemoryManagementFixture::setUp();
        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
        UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->calculateMaxOsContextCount();

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
        memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        csr = &device->getGpgpuCommandStreamReceiver();

        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto engineType = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment())[0].first;
        auto osContext = memoryManager->createAndRegisterOsContext(csr, EngineDescriptorHelper::getDefaultDescriptor({engineType, EngineUsage::regular},
                                                                                                                     PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
        csr->setupContext(*osContext);
    }

    void tearDown() {
        device.reset();
        MemoryManagementFixture::tearDown();
    }

  protected:
    std::unique_ptr<MockDevice> device;
    ExecutionEnvironment *executionEnvironment;
    MockMemoryManager *memoryManager = nullptr;
    CommandStreamReceiver *csr = nullptr;
};
