/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/memory_manager_fixture.h"

#include "core/helpers/hw_helper.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/os_interface/os_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace NEO;

void MemoryManagerWithCsrFixture::SetUp() {
    executionEnvironment.setHwInfo(*platformDevices);
    csr = std::make_unique<MockCommandStreamReceiver>(this->executionEnvironment, 0);
    memoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    csr->tagAddress = &currentGpuTag;
    auto engine = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0];
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), engine, 1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    csr->setupContext(*osContext);
}

void MemoryManagerWithCsrFixture::TearDown() {
}
