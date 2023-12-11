/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/memory_manager_fixture.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

using namespace NEO;

void MemoryManagerWithCsrFixture::setUp() {
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    csr = std::make_unique<MockCommandStreamReceiver>(this->executionEnvironment, 0, 1);
    memoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    csr->tagAddress = &currentGpuTag;
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto engine = gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0];
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({engine.first, EngineUsage::regular},
                                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
    csr->setupContext(*osContext);
}

void MemoryManagerWithCsrFixture::tearDown() {
}
