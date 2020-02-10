/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/device/root_device.h"
#include "core/device/sub_device.h"
#include "core/helpers/hw_helper.h"
#include "core/unit_tests/helpers/default_hw_info.h"
#include "runtime/device/cl_device.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_allocation_properties.h"

namespace NEO {
class OSTime;
class FailMemoryManager;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

struct MockSubDevice : public SubDevice {
    using SubDevice::SubDevice;

    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override {
        return std::unique_ptr<CommandStreamReceiver>(createCommandStreamReceiverFunc(*executionEnvironment, getRootDeviceIndex()));
    }
    static decltype(&createCommandStream) createCommandStreamReceiverFunc;
};

class MockDevice : public RootDevice {
  public:
    using Device::commandStreamReceivers;
    using Device::createDeviceInternals;
    using Device::createEngine;
    using Device::deviceExtensions;
    using Device::deviceInfo;
    using Device::enabledClVersion;
    using Device::engines;
    using Device::executionEnvironment;
    using Device::initializeCaps;
    using Device::name;
    using RootDevice::createEngines;
    using RootDevice::subdevices;

    void setOSTime(OSTime *osTime);
    void setDriverInfo(DriverInfo *driverInfo);
    bool hasDriverInfo();

    static bool createSingleDevice;
    bool createDeviceImpl() override;

    bool getCpuTime(uint64_t *timeStamp) { return true; };
    MockDevice();
    MockDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex);

    void setPreemptionMode(PreemptionMode mode) {
        preemptionMode = mode;
    }

    void injectMemoryManager(MemoryManager *);

    void setPerfCounters(PerformanceCounters *perfCounters) {
        if (perfCounters) {
            performanceCounters = std::unique_ptr<PerformanceCounters>(perfCounters);
        } else {
            performanceCounters.release();
        }
    }

    const char *getProductAbbrev() const;

    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<T> &>(*engines[defaultEngineIndex].commandStreamReceiver);
    }

    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiverFromIndex(uint32_t index) {
        return reinterpret_cast<UltCommandStreamReceiver<T> &>(*engines[index].commandStreamReceiver);
    }
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const { return *engines[defaultEngineIndex].commandStreamReceiver; }
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr);
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex);

    void setDebuggerActive(bool active) {
        this->deviceInfo.debuggerActive = active;
    }

    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) {
        pHwInfo = pHwInfo ? pHwInfo : platformDevices[0];
        executionEnvironment->setHwInfo(pHwInfo);
        T *device = new T(executionEnvironment, rootDeviceIndex);
        executionEnvironment->memoryManager = std::move(device->mockMemoryManager);
        return createDeviceInternals(device);
    }

    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex = 0) {
        ExecutionEnvironment *executionEnvironment = new ExecutionEnvironment();
        auto numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get() ? DebugManager.flags.CreateMultipleRootDevices.get() : 1u;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        pHwInfo = pHwInfo ? pHwInfo : platformDevices[0];
        executionEnvironment->setHwInfo(pHwInfo);
        return createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }

    SubDevice *createSubDevice(uint32_t subDeviceIndex) override {
        return Device::create<MockSubDevice>(executionEnvironment, subDeviceIndex, *this);
    }

    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const override {
        return std::unique_ptr<CommandStreamReceiver>(createCommandStreamReceiverFunc(*executionEnvironment, getRootDeviceIndex()));
    }

    static decltype(&createCommandStream) createCommandStreamReceiverFunc;
    std::unique_ptr<MemoryManager> mockMemoryManager;
};

class MockClDevice : public ClDevice {
  public:
    using ClDevice::ClDevice;
    using ClDevice::simultaneousInterops;

    explicit MockClDevice(MockDevice *pMockDevice);

    bool createEngines() { return device.createEngines(); }
    void setOSTime(OSTime *osTime) { device.setOSTime(osTime); }
    void setDriverInfo(DriverInfo *driverInfo) { device.setDriverInfo(driverInfo); }
    bool hasDriverInfo() { return device.hasDriverInfo(); }
    bool createDeviceImpl() { return device.createDeviceImpl(); }
    bool getCpuTime(uint64_t *timeStamp) { return device.getCpuTime(timeStamp); }
    void setPreemptionMode(PreemptionMode mode) { device.setPreemptionMode(mode); }
    void injectMemoryManager(MemoryManager *pMemoryManager) { device.injectMemoryManager(pMemoryManager); }
    void setPerfCounters(PerformanceCounters *perfCounters) { device.setPerfCounters(perfCounters); }
    const char *getProductAbbrev() const { return device.getProductAbbrev(); }
    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiver() { return device.getUltCommandStreamReceiver<T>(); }
    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiverFromIndex(uint32_t index) { return device.getUltCommandStreamReceiverFromIndex<T>(index); }
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const { return device.getGpgpuCommandStreamReceiver(); }
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr) { device.resetCommandStreamReceiver(newCsr); }
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex) { device.resetCommandStreamReceiver(newCsr, engineIndex); }
    void setSourceLevelDebuggerActive(bool active) { device.setDebuggerActive(active); }
    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) {
        return MockDevice::createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }
    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex = 0) {
        return MockDevice::createWithNewExecutionEnvironment<T>(pHwInfo, rootDeviceIndex);
    }
    SubDevice *createSubDevice(uint32_t subDeviceIndex) { return device.createSubDevice(subDeviceIndex); }
    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const { return device.createCommandStreamReceiver(); }

    MockDevice &device;
    DeviceInfo &deviceInfo;
    ExecutionEnvironment *&executionEnvironment;
    static bool &createSingleDevice;
    static decltype(&createCommandStream) &createCommandStreamReceiverFunc;
    std::vector<SubDevice *> &subdevices;
    std::unique_ptr<MemoryManager> &mockMemoryManager;
    std::vector<EngineControl> &engines;
};

template <>
inline Device *MockDevice::createWithNewExecutionEnvironment<Device>(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex) {
    auto executionEnvironment = new ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    MockAubCenterFixture::setMockAubCenter(*executionEnvironment->rootDeviceEnvironments[0]);
    auto hwInfo = pHwInfo ? pHwInfo : *platformDevices;
    executionEnvironment->setHwInfo(hwInfo);
    executionEnvironment->initializeMemoryManager();
    return Device::create<RootDevice>(executionEnvironment, 0u);
}

class FailDevice : public MockDevice {
  public:
    FailDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

class FailDeviceAfterOne : public MockDevice {
  public:
    FailDeviceAfterOne(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

class MockAlignedMallocManagerDevice : public MockDevice {
  public:
    MockAlignedMallocManagerDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
};

struct EnvironmentWithCsrWrapper {
    template <typename CsrType>
    void setCsrType() {
        createSubDeviceCsrFuncBackup = EnvironmentWithCsrWrapper::createCommandStreamReceiver<CsrType>;
        createRootDeviceCsrFuncBackup = EnvironmentWithCsrWrapper::createCommandStreamReceiver<CsrType>;
    }

    template <typename CsrType>
    static CommandStreamReceiver *createCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
        return new CsrType(executionEnvironment, 0);
    }

    VariableBackup<decltype(MockSubDevice::createCommandStreamReceiverFunc)> createSubDeviceCsrFuncBackup{&MockSubDevice::createCommandStreamReceiverFunc};
    VariableBackup<decltype(MockDevice::createCommandStreamReceiverFunc)> createRootDeviceCsrFuncBackup{&MockDevice::createCommandStreamReceiverFunc};
};
} // namespace NEO
