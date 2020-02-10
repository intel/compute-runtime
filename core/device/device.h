/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/execution_environment/execution_environment.h"
#include "core/helpers/common_types.h"
#include "core/helpers/engine_control.h"
#include "core/helpers/hw_info.h"
#include "runtime/device/device_info.h"
#include "runtime/os_interface/performance_counters.h"

namespace NEO {
class DriverInfo;
class OSTime;

class Device : public ReferenceTrackedObject<Device> {
  public:
    Device &operator=(const Device &) = delete;
    Device(const Device &) = delete;
    ~Device() override;

    unsigned int getEnabledClVersion() const { return enabledClVersion; };
    unsigned int getSupportedClVersion() const;
    void appendOSExtensions(const std::string &newExtensions);

    template <typename DeviceT, typename... ArgsT>
    static DeviceT *create(ArgsT &&... args) {
        DeviceT *device = new DeviceT(std::forward<ArgsT>(args)...);
        return createDeviceInternals(device);
    }

    virtual bool isReleasable() = 0;

    bool getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const;
    bool getHostTimer(uint64_t *hostTimestamp) const;
    const HardwareInfo &getHardwareInfo() const;
    const DeviceInfo &getDeviceInfo() const;
    EngineControl &getEngine(aub_stream::EngineType engineType, bool lowPriority);
    EngineControl &getDefaultEngine();
    EngineControl &getInternalEngine();
    MemoryManager *getMemoryManager() const;
    GmmHelper *getGmmHelper() const;
    OSTime *getOSTime() const { return osTime.get(); };
    double getProfilingTimerResolution();
    double getPlatformHostTimerResolution() const;
    bool isSimulation() const;
    GFXCORE_FAMILY getRenderCoreFamily() const;
    PerformanceCounters *getPerformanceCounters() { return performanceCounters.get(); }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    MOCKABLE_VIRTUAL bool isDebuggerActive() const;
    Debugger *getDebugger() { return executionEnvironment->debugger.get(); }
    ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    const RootDeviceEnvironment &getRootDeviceEnvironment() const { return *executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]; }
    const HardwareCapabilities &getHardwareCapabilities() const { return hardwareCapabilities; }
    bool isFullRangeSvm() const {
        return executionEnvironment->isFullRangeSvm();
    }
    bool areSharedSystemAllocationsAllowed() const {
        return this->deviceInfo.sharedSystemMemCapabilities != 0u;
    }
    template <typename SpecializedDeviceT>
    void setSpecializedDevice(SpecializedDeviceT *specializedDevice) {
        this->specializedDevice = reinterpret_cast<uintptr_t>(specializedDevice);
    }
    template <typename SpecializedDeviceT>
    SpecializedDeviceT *getSpecializedDevice() const {
        return reinterpret_cast<SpecializedDeviceT *>(specializedDevice);
    }

    virtual uint32_t getRootDeviceIndex() const = 0;
    virtual uint32_t getNumAvailableDevices() const = 0;
    virtual Device *getDeviceById(uint32_t deviceId) const = 0;
    virtual DeviceBitfield getDeviceBitfield() const = 0;

    static decltype(&PerformanceCounters::create) createPerformanceCountersFunc;

  protected:
    Device() = delete;
    Device(ExecutionEnvironment *executionEnvironment);

    MOCKABLE_VIRTUAL void initializeCaps();
    void setupFp64Flags();

    template <typename T>
    static T *createDeviceInternals(T *device) {
        if (false == device->createDeviceImpl()) {
            delete device;
            return nullptr;
        }
        return device;
    }

    virtual bool createDeviceImpl();
    virtual bool createEngines();
    bool createEngine(uint32_t deviceCsrIndex, aub_stream::EngineType engineType);
    MOCKABLE_VIRTUAL std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const;

    unsigned int enabledClVersion = 0u;
    std::string deviceExtensions;
    std::string exposedBuiltinKernels = "";

    DeviceInfo deviceInfo;

    std::string name;
    HardwareCapabilities hardwareCapabilities = {};
    std::unique_ptr<OSTime> osTime;
    std::unique_ptr<DriverInfo> driverInfo;
    std::unique_ptr<PerformanceCounters> performanceCounters;
    std::vector<std::unique_ptr<CommandStreamReceiver>> commandStreamReceivers;
    std::vector<EngineControl> engines;
    PreemptionMode preemptionMode;
    ExecutionEnvironment *executionEnvironment = nullptr;
    uint32_t defaultEngineIndex = 0;

    uintptr_t specializedDevice = reinterpret_cast<uintptr_t>(nullptr);
};

inline EngineControl &Device::getDefaultEngine() {
    return engines[defaultEngineIndex];
}

inline MemoryManager *Device::getMemoryManager() const {
    return executionEnvironment->memoryManager.get();
}

inline GmmHelper *Device::getGmmHelper() const {
    return executionEnvironment->getGmmHelper();
}

} // namespace NEO
