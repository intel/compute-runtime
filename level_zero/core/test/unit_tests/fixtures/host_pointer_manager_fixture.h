/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_host_pointer_manager.h"

namespace L0 {
namespace ult {

struct HostPointerManagerFixure {
    void SetUp() {
        NEO::MockCompilerEnableGuard mock(true);
        NEO::DeviceVector devices;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
            std::make_unique<NEO::MockMemoryOperationsHandlerTests>();
        mockMemoryInterface = static_cast<NEO::MockMemoryOperationsHandlerTests *>(
            neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.get());
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        hostDriverHandle = std::make_unique<L0::ult::DriverHandle>();
        hostDriverHandle->initialize(std::move(devices));
        device = hostDriverHandle->devices[0];
        openHostPointerManager = static_cast<L0::ult::HostPointerManager *>(hostDriverHandle->hostPointerManager.get());

        heapPointer = hostDriverHandle->getMemoryManager()->allocateSystemMemory(heapSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, heapPointer);

        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t ret = hostDriverHandle->createContext(&desc, 0u, nullptr, &hContext);
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
    size_t heapSize = 4 * MemoryConstants::pageSize;
};

struct ForceDisabledHostPointerManagerFixure : public HostPointerManagerFixure {
    void SetUp() {
        DebugManager.flags.EnableHostPointerImport.set(0);

        HostPointerManagerFixure::SetUp();
    }

    void TearDown() {
        HostPointerManagerFixure::TearDown();
    }
};

struct ForceEnabledHostPointerManagerFixure : public HostPointerManagerFixure {
    void SetUp() {
        DebugManager.flags.EnableHostPointerImport.set(1);

        HostPointerManagerFixure::SetUp();
    }

    void TearDown() {
        HostPointerManagerFixure::TearDown();
    }
};

} // namespace ult
} // namespace L0
