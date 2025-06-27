/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/cl_device/cl_device.h"

namespace NEO {
class FailMemoryManager;
class OSTime;
class SubDevice;
template <typename GfxFamily>
class UltCommandStreamReceiver;
struct HardwareInfo;
class BuiltIns;
class ExecutionEnvironment;
class MemoryManager;
enum PreemptionMode : uint32_t;
struct DeviceInfo;
struct EngineControl;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

class MockClDevice : public ClDevice {
  public:
    using ClDevice::ClDevice;
    using ClDevice::compilerExtensions;
    using ClDevice::compilerExtensionsWithFeatures;
    using ClDevice::deviceExtensions;
    using ClDevice::deviceInfo;
    using ClDevice::driverInfo;
    using ClDevice::enabledClVersion;
    using ClDevice::getClDeviceName;
    using ClDevice::getQueueFamilyCapabilities;
    using ClDevice::getQueueFamilyCapabilitiesAll;
    using ClDevice::initializeCaps;
    using ClDevice::name;
    using ClDevice::ocl21FeaturesEnabled;
    using ClDevice::simultaneousInterops;
    using ClDevice::subDevices;

    explicit MockClDevice(MockDevice *pMockDevice);

    void setPciUuid(std::array<uint8_t, ProductHelper::uuidSize> &id);
    bool createEngines() { return device.createEngines(); }
    void setOSTime(OSTime *osTime) { device.setOSTime(osTime); }
    bool getCpuTime(uint64_t *timeStamp) { return device.getCpuTime(timeStamp); }
    void setPreemptionMode(PreemptionMode mode) { device.setPreemptionMode(mode); }
    void injectMemoryManager(MemoryManager *pMemoryManager) { device.injectMemoryManager(pMemoryManager); }
    void setPerfCounters(std::unique_ptr<PerformanceCounters> perfCounters) { device.setPerfCounters(std::move(perfCounters)); }
    const char *getProductAbbrev() const { return device.getProductAbbrev(); }
    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiver() { return device.getUltCommandStreamReceiver<T>(); }
    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiverFromIndex(uint32_t index) { return device.getUltCommandStreamReceiverFromIndex<T>(index); }
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const { return device.getGpgpuCommandStreamReceiver(); }
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr) { device.resetCommandStreamReceiver(newCsr); }
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex) { device.resetCommandStreamReceiver(newCsr, engineIndex); }
    template <typename T>
    static T *createWithExecutionEnvironment(const HardwareInfo *pHwInfo, ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) {
        return MockDevice::createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }
    template <typename T>
    static T *createWithNewExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex = 0) {
        auto executionEnvironment = prepareExecutionEnvironment(pHwInfo, rootDeviceIndex);
        return MockDevice::createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }

    static ExecutionEnvironment *prepareExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex);

    SubDevice *createSubDevice(uint32_t subDeviceIndex) { return device.createSubDevice(subDeviceIndex); }
    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const;
    BuiltIns *getBuiltIns() const { return getDevice().getBuiltIns(); }

    bool areOcl21FeaturesSupported() const;

    MockDevice &device;
    DeviceInfo &sharedDeviceInfo;
    ExecutionEnvironment *&executionEnvironment;
    static bool &createSingleDevice;
    static decltype(&createCommandStream) &createCommandStreamReceiverFunc;
    std::vector<EngineControl> &allEngines;
};

class MockDeviceWithDebuggerActive : public MockDevice {
  public:
    MockDeviceWithDebuggerActive(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : MockDevice(executionEnvironment, deviceIndex) {}
    void initializeCaps() override {
        MockDevice::initializeCaps();
    }
};

} // namespace NEO
