/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/unit_test/page_fault_manager/cpu_page_fault_manager_tests_fixture.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace NEO {
struct UltDeviceFactory;
extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
} // namespace NEO

namespace L0 {
struct Context;
struct Device;

namespace ult {
class MockBuiltins;

struct DeviceFixture {
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    virtual void SetUp();    // NOLINT(readability-identifier-naming)
    virtual void TearDown(); // NOLINT(readability-identifier-naming)
    void setupWithExecutionEnvironment(NEO::ExecutionEnvironment &executionEnvironment);

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::ContextImp *context = nullptr;
    MockBuiltins *mockBuiltIns = nullptr;
};

struct PageFaultDeviceFixture {
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    virtual void SetUp();    // NOLINT(readability-identifier-naming)
    virtual void TearDown(); // NOLINT(readability-identifier-naming)

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::unique_ptr<MockMemoryManager> mockMemoryManager;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::ContextImp *context = nullptr;
    MockPageFaultManager *mockPageFaultManager = nullptr;
    NEO::MemoryManager *memoryManager = nullptr;
};

struct MultiDeviceFixture {
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    virtual void SetUp();    // NOLINT(readability-identifier-naming)
    virtual void TearDown(); // NOLINT(readability-identifier-naming)

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::vector<NEO::Device *> devices;
    uint32_t numRootDevices = 4u;
    uint32_t numSubDevices = 2u;
    L0::ContextImp *context = nullptr;
};

struct ContextFixture : DeviceFixture {
    void SetUp() override;
    void TearDown() override;
};

struct AubCsrFixture : ContextFixture {
    template <typename T>
    void SetUpT() {
        auto csrCreateFcn = &commandStreamReceiverFactory[IGFX_MAX_CORE + NEO::defaultHwInfo->platform.eRenderCoreFamily];
        variableBackup = std::make_unique<VariableBackup<CommandStreamReceiverCreateFunc>>(csrCreateFcn);
        *csrCreateFcn = UltAubCommandStreamReceiver<T>::create;
        ContextFixture::SetUp();
    }
    template <typename T>
    void TearDownT() {
        ContextFixture::TearDown();
    }

    void SetUp() override{};
    void TearDown() override{};
    std::unique_ptr<VariableBackup<CommandStreamReceiverCreateFunc>> variableBackup;
};

struct MultipleDevicesWithCustomHwInfo {
    void SetUp();
    void TearDown() {}
    NEO::HardwareInfo hwInfo;
    const uint32_t numSubslicesPerSlice = 4;
    const uint32_t numEuPerSubslice = 8;
    const uint32_t numThreadsPerEu = 7;
    const uint32_t sliceCount = 2;
    const uint32_t subsliceCount = 8;

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::OsAgnosticMemoryManager *memoryManager = nullptr;
    std::unique_ptr<UltDeviceFactory> deviceFactory;

    const uint32_t numRootDevices = 1u;
    const uint32_t numSubDevices = 2u;
};

} // namespace ult
} // namespace L0
