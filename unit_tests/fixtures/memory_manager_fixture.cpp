/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/os_context.h"
#include "unit_tests/fixtures/memory_manager_fixture.h"
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
    csr->setupContext(*memoryManager->createAndRegisterOsContext(HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], 1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0])));
}

void MemoryManagerWithCsrFixture::TearDown() {
}
