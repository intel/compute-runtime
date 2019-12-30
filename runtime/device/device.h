/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/common_types.h"
#include "core/helpers/engine_control.h"
#include "core/helpers/hw_info.h"
#include "runtime/api/cl_types.h"
#include "runtime/device/device_info_map.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/base_object.h"
#include "runtime/os_interface/performance_counters.h"

#include "engine_node.h"

namespace NEO {
class DriverInfo;
class OSTime;
class SyncBufferHandler;

template <>
struct OpenCLObjectMapper<_cl_device_id> {
    typedef class Device DerivedType;
};

class Device : public BaseObject<_cl_device_id> {
  public:
    static const cl_ulong objectMagic = 0x8055832341AC8D08LL;

    Device &operator=(const Device &) = delete;
    Device(const Device &) = delete;
    ~Device() override;

    // API entry points
    cl_int getDeviceInfo(cl_device_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet);

    // This helper template is meant to simplify getDeviceInfo
    template <cl_device_info Param>
    void getCap(const void *&src,
                size_t &size,
                size_t &retSize);

    template <cl_device_info Param>
    void getStr(const void *&src,
                size_t &size,
                size_t &retSize);
    unsigned int getEnabledClVersion() const { return enabledClVersion; };
    unsigned int getSupportedClVersion() const;

    template <typename DeviceT, typename... ArgsT>
    static DeviceT *create(ArgsT &&... args) {
        DeviceT *device = new DeviceT(std::forward<ArgsT>(args)...);
        return createDeviceInternals(device);
    }

    bool getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const;
    bool getHostTimer(uint64_t *hostTimestamp) const;
    const HardwareInfo &getHardwareInfo() const;
    const DeviceInfo &getDeviceInfo() const;
    EngineControl &getEngine(aub_stream::EngineType engineType, bool lowPriority);
    EngineControl &getDefaultEngine();
    MemoryManager *getMemoryManager() const;
    GmmHelper *getGmmHelper() const;
    OSTime *getOSTime() const { return osTime.get(); };
    double getProfilingTimerResolution();
    double getPlatformHostTimerResolution() const;
    bool isSimulation() const;
    GFXCORE_FAMILY getRenderCoreFamily() const;
    void allocateSyncBufferHandler();
    PerformanceCounters *getPerformanceCounters() { return performanceCounters.get(); }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    MOCKABLE_VIRTUAL bool isSourceLevelDebuggerActive() const;
    SourceLevelDebugger *getSourceLevelDebugger() { return executionEnvironment->sourceLevelDebugger.get(); }
    ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    const RootDeviceEnvironment &getRootDeviceEnvironment() const { return *executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]; }
    const HardwareCapabilities &getHardwareCapabilities() const { return hardwareCapabilities; }
    bool isFullRangeSvm() const {
        return executionEnvironment->isFullRangeSvm();
    }
    bool areSharedSystemAllocationsAllowed() const {
        return this->deviceInfo.sharedSystemMemCapabilities != 0u;
    }

    virtual uint32_t getRootDeviceIndex() const = 0;
    virtual uint32_t getNumAvailableDevices() const = 0;
    virtual Device *getDeviceById(uint32_t deviceId) const = 0;
    virtual DeviceBitfield getDeviceBitfield() const = 0;

    static decltype(&PerformanceCounters::create) createPerformanceCountersFunc;
    std::unique_ptr<SyncBufferHandler> syncBufferHandler;

  protected:
    Device() = delete;
    Device(ExecutionEnvironment *executionEnvironment);

    MOCKABLE_VIRTUAL void initializeCaps();
    void setupFp64Flags();
    void appendOSExtensions(std::string &deviceExtensions);

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

    std::vector<unsigned int> simultaneousInterops;
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
};

template <cl_device_info Param>
inline void Device::getCap(const void *&src,
                           size_t &size,
                           size_t &retSize) {
    src = &DeviceInfoTable::Map<Param>::getValue(deviceInfo);
    retSize = size = DeviceInfoTable::Map<Param>::size;
}

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
