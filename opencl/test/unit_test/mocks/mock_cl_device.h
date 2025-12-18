/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/cl_device/cl_device.h"

namespace NEO {
class BuiltinDispatchInfoBuilder;
class CommandStreamReceiver;
class FailMemoryManager;
class OSTime;
class SubDevice;
template <typename GfxFamily>
class UltCommandStreamReceiver;
struct HardwareInfo;
class BuiltIns;
class MockClExecutionEnvironment;
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
    using ClDevice::getClDeviceName;
    using ClDevice::getQueueFamilyCapabilities;
    using ClDevice::getQueueFamilyCapabilitiesAll;
    using ClDevice::initializeCaps;
    using ClDevice::name;
    using ClDevice::simultaneousInterops;
    using ClDevice::subDevices;

    explicit MockClDevice(MockDevice *pMockDevice);

    void setPciUuid(std::array<uint8_t, ProductHelper::uuidSize> &id);
    bool createEngines();
    void setOSTime(OSTime *osTime);
    bool getCpuTime(uint64_t *timeStamp);
    void setPreemptionMode(PreemptionMode mode);
    void injectMemoryManager(MemoryManager *pMemoryManager);
    void setPerfCounters(std::unique_ptr<PerformanceCounters> perfCounters);
    const char *getProductAbbrev() const;
    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiver() { return reinterpret_cast<UltCommandStreamReceiver<T> &>(getGpgpuCommandStreamReceiver()); }
    template <typename T>
    UltCommandStreamReceiver<T> &getUltCommandStreamReceiverFromIndex(uint32_t index) { return reinterpret_cast<UltCommandStreamReceiver<T> &>(*allEngines[index].commandStreamReceiver); }
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const;
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr);
    void resetCommandStreamReceiver(CommandStreamReceiver *newCsr, uint32_t engineIndex);

    static MockClExecutionEnvironment *prepareExecutionEnvironment(const HardwareInfo *pHwInfo, uint32_t rootDeviceIndex);

    SubDevice *createSubDevice(uint32_t subDeviceIndex);
    std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const;
    BuiltIns *getBuiltIns() const;
    std::unique_ptr<BuiltinDispatchInfoBuilder> setBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, std::unique_ptr<BuiltinDispatchInfoBuilder> builder);

    MockDevice &device;
    DeviceInfo &sharedDeviceInfo;
    ExecutionEnvironment *&executionEnvironment;
    static bool &createSingleDevice;
    static decltype(&createCommandStream) &createCommandStreamReceiverFunc;
    std::vector<EngineControl> &allEngines;
};

} // namespace NEO
