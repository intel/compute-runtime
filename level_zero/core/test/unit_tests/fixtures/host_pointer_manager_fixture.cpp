/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_host_pointer_manager.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

void HostPointerManagerFixure::setUp() {

    NEO::DeviceVector devices;
    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperationsHandlerTests>();
    mockMemoryInterface = static_cast<NEO::MockMemoryOperationsHandlerTests *>(
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    hostDriverHandle = std::make_unique<L0::ult::DriverHandle>();
    hostDriverHandle->initialize(std::move(devices));
    device = hostDriverHandle->devices[0];
    if (neoDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
        for (auto &engine : neoDevice->getAllEngines()) {
            NEO::CommandStreamReceiver *csr = engine.commandStreamReceiver;
            if (!csr->getPreemptionAllocation()) {
                csr->createPreemptionAllocation();
            }
        }
    }

    openHostPointerManager = static_cast<L0::ult::HostPointerManager *>(hostDriverHandle->hostPointerManager.get());

    heapPointer = hostDriverHandle->getMemoryManager()->allocateSystemMemory(heapSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, heapPointer);

    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t ret = hostDriverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    context = L0::Context::fromHandle(hContext);
}

void HostPointerManagerFixure::tearDown() {
    context->destroy();
    hostDriverHandle->getMemoryManager()->freeSystemMemory(heapPointer);
    hostDriverHandle->svmAllocsManager->cleanupUSMAllocCaches();
}

} // namespace ult
} // namespace L0
