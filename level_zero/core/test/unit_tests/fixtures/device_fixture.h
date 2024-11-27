/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"

class MockPageFaultManager;
namespace NEO {
struct UltDeviceFactory;
class MockMemoryManager;
class OsAgnosticMemoryManager;
class MemoryManagerMemHandleMock;
} // namespace NEO

namespace L0 {
struct Context;
struct Device;
struct ContextImp;

extern uint32_t driverCount;
extern _ze_driver_handle_t *globalDriverHandle;
namespace ult {
class MockBuiltins;

struct DeviceFixture {
    virtual ~DeviceFixture() = default;
    void setUp();
    void setUpImpl(NEO::HardwareInfo *hwInfo);
    void tearDown();
    void setupWithExecutionEnvironment(NEO::ExecutionEnvironment &executionEnvironment);

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::ContextImp *context = nullptr;
    MockBuiltins *mockBuiltIns = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
    HardwareInfo *hardwareInfo = nullptr;

    const uint32_t rootDeviceIndex = 0u;
    template <typename HelperType>
    HelperType &getHelper() const;
    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&globalDriverHandle};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
};

template <typename T>
struct DeviceFixtureWithCustomMemoryManager : public DeviceFixture {
    void setUp() {
        auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u);
        auto memoryManager = new T(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }
};

struct DriverHandleGetMemHandlePtrMock : public L0::DriverHandleImp {
    void *importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAloc, NEO::SvmAllocationData &mappedPeerAllocData) override {
        if (failHandleLookup) {
            return nullptr;
        }
        return &mockFd;
    }

    void *importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType) override {
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

    void setUp();
    void tearDown();

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    uint32_t numRootDevices = 4u;
    uint32_t numSubDevices = 2u;
    L0::ContextImp *context = nullptr;

    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&globalDriverHandle};
    VariableBackup<uint32_t> driverCountBackup{&driverCount};
};

struct MultiDeviceFixtureHierarchy : public MultiDeviceFixture {
    void setUp();
    bool exposeSubDevices = true;
};

struct MultiDeviceFixtureCombinedHierarchy : public MultiDeviceFixture {
    void setUp();
    bool exposeSubDevices = true;
    bool combinedHierarchy = true;
};

struct SingleRootMultiSubDeviceFixture : public MultiDeviceFixture {
    void setUp();

    L0::Device *device = nullptr;
    NEO::Device *neoDevice = nullptr;
};

struct ImplicitScalingRootDevice : public SingleRootMultiSubDeviceFixture {
    void setUp() {
        debugManager.flags.EnableImplicitScaling.set(1);
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
    uint32_t numSubDevices = 2u;
};

struct SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl : public MultiDeviceFixture {

    SingleRootMultiSubDeviceFixtureWithImplicitScalingImpl(uint32_t copyEngineCount, uint32_t implicitScaling) : implicitScaling(implicitScaling), expectedCopyEngineCount(copyEngineCount){};

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

class FalseGpuCpuDeviceTime : public NEO::DeviceTime {
  public:
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        return TimeQueryStatus::deviceLost;
    }
    double getDynamicDeviceTimerResolution() const override {
        return NEO::OSTime::getDeviceTimerResolution();
    }
    uint64_t getDynamicDeviceTimerClock() const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution());
    }
};

class FalseGpuCpuTime : public NEO::OSTime {
  public:
    FalseGpuCpuTime() {
        this->deviceTime = std::make_unique<FalseGpuCpuDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        return true;
    };
    double getHostTimerResolution() const override {
        return 0;
    }
    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
    static std::unique_ptr<OSTime> create() {
        return std::unique_ptr<OSTime>(new FalseGpuCpuTime());
    }
};

class FalseUnSupportedFeatureGpuCpuDeviceTime : public NEO::DeviceTime {
  public:
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        return TimeQueryStatus::unsupportedFeature;
    }
    double getDynamicDeviceTimerResolution() const override {
        return NEO::OSTime::getDeviceTimerResolution();
    }
    uint64_t getDynamicDeviceTimerClock() const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution());
    }
};

class FalseUnSupportedFeatureGpuCpuTime : public NEO::OSTime {
  public:
    FalseUnSupportedFeatureGpuCpuTime() {
        this->deviceTime = std::make_unique<FalseUnSupportedFeatureGpuCpuDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        return true;
    };
    double getHostTimerResolution() const override {
        return 0;
    }
    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
    static std::unique_ptr<OSTime> create() {
        return std::unique_ptr<OSTime>(new FalseGpuCpuTime());
    }
};

} // namespace ult
} // namespace L0
