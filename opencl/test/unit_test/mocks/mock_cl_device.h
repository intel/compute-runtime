/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"

namespace NEO {
class FailMemoryManager;
class OSTime;
class SubDevice;
template <typename GfxFamily>
class UltCommandStreamReceiver;
struct HardwareInfo;

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

    bool createEngines() { return device.createEngines(); }
    void setOSTime(OSTime *osTime) { device.setOSTime(osTime); }
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
        auto executionEnvironment = prepareExecutionEnvironment(pHwInfo, rootDeviceIndex);
        return MockDevice::createWithExecutionEnvironment<T>(pHwInfo, executionEnvironment, rootDeviceIndex);
    }
    static ExecutionEnvironment *prepareExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex) {
        auto executionEnvironment = new MockClExecutionEnvironment();
        auto numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get() ? DebugManager.flags.CreateMultipleRootDevices.get() : rootDeviceIndex + 1;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        pHwInfo = pHwInfo ? pHwInfo : defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(pHwInfo);
        }
        executionEnvironment->calculateMaxOsContextCount();
        return executionEnvironment;
    }
    SubDevice *createSubDevice(uint32_t subDeviceIndex) { return device.createSubDevice(subDeviceIndex); }
    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const { return device.createCommandStreamReceiver(); }
    BuiltIns *getBuiltIns() const { return getDevice().getBuiltIns(); }

    bool areOcl21FeaturesSupported() const;

    void setDebuggerActive(bool active) {
        sharedDeviceInfo.debuggerActive = active;
    }

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
        this->setDebuggerActive(true);
    }
};

} // namespace NEO
