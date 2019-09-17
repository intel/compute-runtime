/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/device/device_info_map.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/common_types.h"
#include "runtime/helpers/engine_control.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/performance_counters.h"

#include "engine_node.h"

namespace NEO {
class OSTime;
class DriverInfo;

template <>
struct OpenCLObjectMapper<_cl_device_id> {
    typedef class Device DerivedType;
};

class Device : public BaseObject<_cl_device_id> {
  public:
    static const cl_ulong objectMagic = 0x8055832341AC8D08LL;

    template <typename DeviceT, typename... ArgsT>
    static DeviceT *create(ArgsT &&... args) {
        DeviceT *device = new DeviceT(std::forward<ArgsT>(args)...);
        return createDeviceInternals(device);
    }

    Device &operator=(const Device &) = delete;
    Device(const Device &) = delete;

    ~Device() override;

    // API entry points
    cl_int getDeviceInfo(cl_device_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet);

    bool getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const;
    bool getHostTimer(uint64_t *hostTimestamp) const;

    // Helper functions
    const HardwareInfo &getHardwareInfo() const;
    const DeviceInfo &getDeviceInfo() const;

    EngineControl &getEngine(aub_stream::EngineType engineType, bool lowPriority);
    EngineControl &getDefaultEngine();

    const char *getProductAbbrev() const;

    // This helper template is meant to simplify getDeviceInfo
    template <cl_device_info Param>
    void getCap(const void *&src,
                size_t &size,
                size_t &retSize);

    template <cl_device_info Param>
    void getStr(const void *&src,
                size_t &size,
                size_t &retSize);

    MemoryManager *getMemoryManager() const;
    GmmHelper *getGmmHelper() const;

    OSTime *getOSTime() const { return osTime.get(); };
    double getProfilingTimerResolution();
    unsigned int getEnabledClVersion() const { return enabledClVersion; };
    unsigned int getSupportedClVersion() const;
    double getPlatformHostTimerResolution() const;
    bool isSimulation() const;
    GFXCORE_FAMILY getRenderCoreFamily() const;
    PerformanceCounters *getPerformanceCounters() { return performanceCounters.get(); }
    static decltype(&PerformanceCounters::create) createPerformanceCountersFunc;
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    std::vector<unsigned int> simultaneousInterops;
    std::string deviceExtensions;
    std::string name;
    MOCKABLE_VIRTUAL bool isSourceLevelDebuggerActive() const;
    SourceLevelDebugger *getSourceLevelDebugger() { return executionEnvironment->sourceLevelDebugger.get(); }
    ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    const HardwareCapabilities &getHardwareCapabilities() const { return hardwareCapabilities; }
    uint32_t getDeviceIndex() const { return deviceIndex; }
    bool isFullRangeSvm() const {
        return executionEnvironment->isFullRangeSvm();
    }
    bool areSharedSystemAllocationsAllowed() const {
        return this->deviceInfo.sharedSystemMemCapabilities != 0u;
    }

  protected:
    Device() = delete;
    Device(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);

    template <typename T>
    static T *createDeviceInternals(T *device) {
        if (false == device->createDeviceImpl()) {
            delete device;
            return nullptr;
        }
        return device;
    }

    virtual bool createDeviceImpl();
    virtual DeviceBitfield getDeviceBitfieldForOsContext() const = 0;
    bool createEngines();
    bool createEngine(uint32_t deviceIndex, uint32_t deviceCsrIndex, aub_stream::EngineType engineType);

    MOCKABLE_VIRTUAL void initializeCaps();
    void setupFp64Flags();
    void appendOSExtensions(std::string &deviceExtensions);
    unsigned int enabledClVersion = 0u;

    HardwareCapabilities hardwareCapabilities = {};
    DeviceInfo deviceInfo;
    std::unique_ptr<OSTime> osTime;
    std::unique_ptr<DriverInfo> driverInfo;
    std::unique_ptr<PerformanceCounters> performanceCounters;

    std::vector<EngineControl> engines;

    std::string exposedBuiltinKernels = "";

    PreemptionMode preemptionMode;
    ExecutionEnvironment *executionEnvironment = nullptr;
    const uint32_t deviceIndex;
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
