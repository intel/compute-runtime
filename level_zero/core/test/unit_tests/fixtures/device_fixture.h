/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

class MockPageFaultManager;
namespace NEO {
struct UltDeviceFactory;
class MockMemoryManager;
class MemoryManagerMemHandleMock;
} // namespace NEO

namespace L0 {
struct Context;
struct Device;
struct ContextImp;

namespace ult {
class MockBuiltins;

struct DeviceFixture {
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    void setUp();
    void tearDown();
    void setupWithExecutionEnvironment(NEO::ExecutionEnvironment &executionEnvironment);

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::ContextImp *context = nullptr;
    MockBuiltins *mockBuiltIns = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
};

struct DriverHandleGetMemHandlePtrMock : public L0::DriverHandleImp {
    void *importFdHandle(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAloc) override {
        if (failHandleLookup) {
            return nullptr;
        }
        return &mockFd;
    }

    void *importNTHandle(ze_device_handle_t hDevice, void *handle) override {
        if (failHandleLookup) {
            return nullptr;
        }
        return &mockHandle;
    }

    uint64_t mockHandle = 57;
    int mockFd = 57;
    bool failHandleLookup = false;
};

struct GetMemHandlePtrTestFixture {
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    void setUp();
    void tearDown();
    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerMemHandleMock *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleGetMemHandlePtrMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<L0::ContextImp> context;
};

struct PageFaultDeviceFixture {
    PageFaultDeviceFixture();
    ~PageFaultDeviceFixture();
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);
    void setUp();
    void tearDown();

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
    void setUp();
    void tearDown();

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::vector<NEO::Device *> devices;
    uint32_t numRootDevices = 4u;
    uint32_t numSubDevices = 2u;
    L0::ContextImp *context = nullptr;
};

struct SingleRootMultiSubDeviceFixture : public MultiDeviceFixture {
    void setUp();

    L0::Device *device = nullptr;
    NEO::Device *neoDevice = nullptr;
};

struct ImplicitScalingRootDevice : public SingleRootMultiSubDeviceFixture {
    void setUp() {
        DebugManager.flags.EnableImplicitScaling.set(1);
        SingleRootMultiSubDeviceFixture::setUp();
    }
};

struct MultipleDevicesWithCustomHwInfo {
    void setUp();
    void tearDown() {}
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

struct SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl : public MultiDeviceFixture {

    SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl(uint32_t copyEngineCount, uint32_t implicitScaling) : implicitScaling(implicitScaling), expectedCopyEngineCount(copyEngineCount){};
    NEO::MockCompilerEnableGuard compilerMock = NEO::MockCompilerEnableGuard(true);

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::vector<NEO::Device *> devices;
    uint32_t numRootDevices = 1u;
    uint32_t numSubDevices = 2u;
    L0::ContextImp *context = nullptr;

    L0::Device *device = nullptr;
    NEO::Device *neoDevice = nullptr;
    L0::DeviceImp *deviceImp = nullptr;

    NEO::HardwareInfo hwInfo{};
    const uint32_t implicitScaling;
    const uint32_t expectedCopyEngineCount;
    uint32_t expectedComputeEngineCount = 0;

    uint32_t numEngineGroups = 0;
    uint32_t subDeviceNumEngineGroups = 0;

    void setUp();
    void tearDown();
};
template <uint32_t copyEngineCount, uint32_t implicitScalingArg>
struct SingleRootMultiSubDeviceFixtureWithImplicitScaling : public SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl {
    SingleRootMultiSubDeviceFixtureWithImplicitScaling() : SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl(copyEngineCount, implicitScalingArg){};
};

} // namespace ult
} // namespace L0
