/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/device/device_info_map.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/engine_node.h"
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

template <>
struct OpenCLObjectMapper<_cl_device_id> {
    typedef class Device DerivedType;
};

class Device : public BaseObject<_cl_device_id> {
  public:
    static const cl_ulong objectMagic = 0x8055832341AC8D08LL;

    template <typename T>
    static T *create(const HardwareInfo *pHwInfo, ExecutionEnvironment *execEnv) {
        pHwInfo = getDeviceInitHwInfo(pHwInfo);
        T *device = new T(*pHwInfo, execEnv);
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
    CommandStreamReceiver *peekCommandStreamReceiver();

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
    void increaseProgramCount() { programCount++; }
    uint64_t getProgramCount() { return programCount; }
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

  protected:
    Device() = delete;
    Device(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment);

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

    unsigned int enabledClVersion;

    const HardwareInfo &hwInfo;
    DeviceInfo deviceInfo;

    volatile uint32_t *tagAddress;
    GraphicsAllocation *preemptionAllocation;
    std::unique_ptr<OSTime> osTime;
    std::unique_ptr<DriverInfo> driverInfo;
    std::unique_ptr<PerformanceCounters> performanceCounters;
    uint64_t programCount = 0u;

    void *slmWindowStartAddress;

    std::string exposedBuiltinKernels = "";

    PreemptionMode preemptionMode;
    EngineType engineType;
    ExecutionEnvironment *executionEnvironment = nullptr;
};

template <cl_device_info Param>
inline void Device::getCap(const void *&src,
                           size_t &size,
                           size_t &retSize) {
    src = &DeviceInfoTable::Map<Param>::getValue(deviceInfo);
    retSize = size = DeviceInfoTable::Map<Param>::size;
}

inline CommandStreamReceiver &Device::getCommandStreamReceiver() {
    return *executionEnvironment->commandStreamReceiver;
}

inline CommandStreamReceiver *Device::peekCommandStreamReceiver() {
    return executionEnvironment->commandStreamReceiver.get();
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
