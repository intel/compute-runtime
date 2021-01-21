/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_memory_operations_handler.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_host_pointer_manager.h"

namespace L0 {
namespace ult {

struct HostPointerManagerFixure {
    void SetUp() {
        NEO::DeviceVector devices;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
            std::make_unique<NEO::MockMemoryOperationsHandlerTests>();
        mockMemoryInterface = static_cast<NEO::MockMemoryOperationsHandlerTests *>(
            neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        DebugManager.flags.EnableHostPointerImport.set(1);
        hostDriverHandle = std::make_unique<L0::ult::DriverHandle>();
        hostDriverHandle->initialize(std::move(devices));
        device = hostDriverHandle->devices[0];
        EXPECT_NE(nullptr, hostDriverHandle->hostPointerManager.get());
        openHostPointerManager = static_cast<L0::ult::HostPointerManager *>(hostDriverHandle->hostPointerManager.get());

        heapPointer = hostDriverHandle->getMemoryManager()->allocateSystemMemory(4 * MemoryConstants::pageSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, heapPointer);

        ze_context_desc_t desc;
        ze_result_t ret = hostDriverHandle->createContext(&desc, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
        context = L0::Context::fromHandle(hContext);
    }

    void TearDown() {
        context->destroy();

        hostDriverHandle->getMemoryManager()->freeSystemMemory(heapPointer);
    }
    DebugManagerStateRestore debugRestore;

    std::unique_ptr<L0::ult::DriverHandle> hostDriverHandle;

    L0::ult::HostPointerManager *openHostPointerManager = nullptr;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    NEO::MockMemoryOperationsHandlerTests *mockMemoryInterface = nullptr;
    ze_context_handle_t hContext;
    L0::Context *context;

    void *heapPointer = nullptr;
};

} // namespace ult
} // namespace L0
