/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/memory_manager_fixture.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

using namespace NEO;

void MemoryManagerWithCsrFixture::setUp() {
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    csr = std::make_unique<MockCommandStreamReceiver>(this->executionEnvironment, 0, 1);
    memoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    csr->tagAddress = &currentGpuTag;
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    auto engine = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({engine.first, EngineUsage::Regular},
                                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
    csr->setupContext(*osContext);
}

void MemoryManagerWithCsrFixture::tearDown() {
}
