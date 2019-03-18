/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/memory_manager_fixture.h"

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/os_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace OCLRT;

void MemoryManagerWithCsrFixture::SetUp() {
    csr = new MockCommandStreamReceiver(this->executionEnvironment);
    memoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    csr->tagAddress = &currentGpuTag;
    executionEnvironment.commandStreamReceivers.resize(1);
    executionEnvironment.commandStreamReceivers[0].push_back(std::unique_ptr<CommandStreamReceiver>(csr));
    auto engine = HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0];
    auto osContext = memoryManager->createAndRegisterOsContext(csr, engine, 1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    csr->setupContext(*osContext);
}

void MemoryManagerWithCsrFixture::TearDown() {
}
