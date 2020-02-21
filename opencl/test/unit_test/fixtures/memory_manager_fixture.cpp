/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/memory_manager_fixture.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

using namespace NEO;

void MemoryManagerWithCsrFixture::SetUp() {
    executionEnvironment.setHwInfo(*platformDevices);
    executionEnvironment.prepareRootDeviceEnvironments(1);
    csr = std::make_unique<MockCommandStreamReceiver>(this->executionEnvironment, 0);
    memoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    csr->tagAddress = &currentGpuTag;
    auto hwInfo = executionEnvironment.getHardwareInfo();
    auto engine = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), engine, 1, PreemptionHelper::getDefaultPreemptionMode(*hwInfo), false);
    csr->setupContext(*osContext);
}

void MemoryManagerWithCsrFixture::TearDown() {
}
