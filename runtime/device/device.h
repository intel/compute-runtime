/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/device/device_info_map.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/engine_node.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/performance_counters.h"
#include <vector>

namespace OCLRT {

class CommandStreamReceiver;
class GraphicsAllocation;
class MemoryManager;
class OSTime;
class DriverInfo;
struct HardwareInfo;
class SourceLevelDebugger;
class OsContext;

template <>
struct OpenCLObjectMapper<_cl_device_id> {
    typedef class Device DerivedType;
};

class Device : public BaseObject<_cl_device_id> {
  public:
    static const cl_ulong objectMagic = 0x8055832341AC8D08LL;

    template <typename T>
    static T *create(const HardwareInfo *pHwInfo, ExecutionEnvironment *execEnv, uint32_t deviceIndex) {
        pHwInfo = getDeviceInitHwInfo(pHwInfo);
        T *device = new T(*pHwInfo, execEnv, deviceIndex);
        return createDeviceInternals(pHwInfo, device);
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
    DeviceInfo *getMutableDeviceInfo();
    MOCKABLE_VIRTUAL const WorkaroundTable *getWaTable() const;
    EngineType getEngineType() const {
        return engineType;
    }

    void *getSLMWindowStartAddress();
    void prepareSLMWindow();
    void setForce32BitAddressing(bool value) {
        deviceInfo.force32BitAddressess = value;
    }

    CommandStreamReceiver &getCommandStreamReceiver();

    volatile uint32_t *getTagAddress() const;

    const char *getProductAbbrev() const;
    const std::string getFamilyNameWithType() const;

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

    /* We hide the retain and release function of BaseObject. */
    void retain() override;
    unique_ptr_if_unused<Device> release() override;
    OSTime *getOSTime() const { return osTime.get(); };
    double getProfilingTimerResolution();
    unsigned int getEnabledClVersion() const { return enabledClVersion; };
    unsigned int getSupportedClVersion() const;
    double getPlatformHostTimerResolution() const;
    bool isSimulation() const;
    void checkPriorityHints();
    GFXCORE_FAMILY getRenderCoreFamily() const;
    PerformanceCounters *getPerformanceCounters() { return performanceCounters.get(); }
    static decltype(&PerformanceCounters::create) createPerformanceCountersFunc;
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    GraphicsAllocation *getPreemptionAllocation() const { return preemptionAllocation; }
    MOCKABLE_VIRTUAL const WhitelistedRegisters &getWhitelistedRegisters() { return hwInfo.capabilityTable.whitelistedRegisters; }
    std::vector<unsigned int> simultaneousInterops;
    std::string deviceExtensions;
    std::string name;
    bool getEnabled64kbPages();
    bool isSourceLevelDebuggerActive() const;
    SourceLevelDebugger *getSourceLevelDebugger() { return executionEnvironment->sourceLevelDebugger.get(); }
    ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    const HardwareCapabilities &getHardwareCapabilities() const { return hardwareCapabilities; }
    OsContext *getOsContext() const { return osContext; }
    uint32_t getDeviceIndex() { return deviceIndex; }
    bool isFullRangeSvm() {
        return getHardwareInfo().capabilityTable.gpuAddressSpace == MemoryConstants::max48BitAddress;
    }

  protected:
    Device() = delete;
    Device(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);

    template <typename T>
    static T *createDeviceInternals(const HardwareInfo *pHwInfo, T *device) {
        if (false == createDeviceImpl(pHwInfo, *device)) {
            delete device;
            return nullptr;
        }
        return device;
    }

    static bool createDeviceImpl(const HardwareInfo *pHwInfo, Device &outDevice);
    static const HardwareInfo *getDeviceInitHwInfo(const HardwareInfo *pHwInfoIn);
    MOCKABLE_VIRTUAL void initializeCaps();
    void setupFp64Flags();
    void appendOSExtensions(std::string &deviceExtensions);

    unsigned int enabledClVersion = 0u;

    const HardwareInfo &hwInfo;
    HardwareCapabilities hardwareCapabilities = {};
    DeviceInfo deviceInfo;

    volatile uint32_t *tagAddress = nullptr;
    GraphicsAllocation *preemptionAllocation = nullptr;
    std::unique_ptr<OSTime> osTime;
    std::unique_ptr<DriverInfo> driverInfo;
    std::unique_ptr<PerformanceCounters> performanceCounters;

    OsContext *osContext = nullptr;

    void *slmWindowStartAddress = nullptr;

    std::string exposedBuiltinKernels = "";

    PreemptionMode preemptionMode;
    EngineType engineType;
    ExecutionEnvironment *executionEnvironment = nullptr;
    uint32_t deviceIndex = 0u;
    CommandStreamReceiver *commandStreamReceiver = nullptr;
};

template <cl_device_info Param>
inline void Device::getCap(const void *&src,
                           size_t &size,
                           size_t &retSize) {
    src = &DeviceInfoTable::Map<Param>::getValue(deviceInfo);
    retSize = size = DeviceInfoTable::Map<Param>::size;
}

inline CommandStreamReceiver &Device::getCommandStreamReceiver() {
    return *this->commandStreamReceiver;
}

inline volatile uint32_t *Device::getTagAddress() const {
    return tagAddress;
}

inline MemoryManager *Device::getMemoryManager() const {
    return executionEnvironment->memoryManager.get();
}

inline GmmHelper *Device::getGmmHelper() const {
    return executionEnvironment->getGmmHelper();
}

} // namespace OCLRT
