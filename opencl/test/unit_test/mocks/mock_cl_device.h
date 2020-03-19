/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

namespace NEO {
class FailMemoryManager;
class OSTime;
class SubDevice;
template <typename GfxFamily>
class UltCommandStreamReceiver;
struct HardwareInfo;

extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

class MockClDevice : public ClDevice {
  public:
    using ClDevice::ClDevice;
    using ClDevice::deviceExtensions;
    using ClDevice::deviceInfo;
    using ClDevice::driverInfo;
    using ClDevice::enabledClVersion;
    using ClDevice::initializeCaps;
    using ClDevice::name;
    using ClDevice::simultaneousInterops;

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
        return MockDevice::createWithNewExecutionEnvironment<T>(pHwInfo, rootDeviceIndex);
    }
    SubDevice *createSubDevice(uint32_t subDeviceIndex) { return device.createSubDevice(subDeviceIndex); }
    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const { return device.createCommandStreamReceiver(); }
    BuiltIns *getBuiltIns() const { return getDevice().getBuiltIns(); }

    void setDebuggerActive(bool active) {
        sharedDeviceInfo.debuggerActive = active;
    }

    MockDevice &device;
    DeviceInfo &sharedDeviceInfo;
    ExecutionEnvironment *&executionEnvironment;
    static bool &createSingleDevice;
    static decltype(&createCommandStream) &createCommandStreamReceiverFunc;
    std::vector<SubDevice *> &subdevices;
    std::unique_ptr<MemoryManager> &mockMemoryManager;
    std::vector<EngineControl> &engines;
};

} // namespace NEO
