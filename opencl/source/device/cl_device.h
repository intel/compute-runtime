/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include "opencl/source/api/cl_types.h"
#include "opencl/source/device/device_info.h"
#include "opencl/source/helpers/base_object.h"

#include "engine_node.h"
#include "igfxfmid.h"

#include <vector>

namespace NEO {
class Debugger;
class Device;
class DriverInfo;
class ExecutionEnvironment;
class GmmHelper;
class GmmClientContext;
class MemoryManager;
class PerformanceCounters;
class Platform;
class SourceLevelDebugger;
class SyncBufferHandler;
struct EngineControl;
struct HardwareCapabilities;
struct HardwareInfo;
struct RootDeviceEnvironment;

template <>
struct OpenCLObjectMapper<_cl_device_id> {
    typedef class ClDevice DerivedType;
};

class ClDevice : public BaseObject<_cl_device_id> {
  public:
    static const cl_ulong objectMagic = 0x8055832341AC8D08LL;

    ClDevice &operator=(const ClDevice &) = delete;
    ClDevice(const ClDevice &) = delete;

    explicit ClDevice(Device &device, Platform *platformId);
    ~ClDevice() override;

    unsigned int getEnabledClVersion() const { return enabledClVersion; };
    unsigned int getSupportedClVersion() const;

    void retainApi();
    unique_ptr_if_unused<ClDevice> releaseApi();

    bool getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const;
    bool getHostTimer(uint64_t *hostTimestamp) const;
    const HardwareInfo &getHardwareInfo() const;
    EngineControl &getEngine(aub_stream::EngineType engineType, bool lowPriority);
    EngineControl &getDefaultEngine();
    EngineControl &getInternalEngine();
    std::atomic<uint32_t> &getSelectorCopyEngine();
    MemoryManager *getMemoryManager() const;
    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;
    double getProfilingTimerResolution();
    double getPlatformHostTimerResolution() const;
    bool isSimulation() const;
    GFXCORE_FAMILY getRenderCoreFamily() const;
    void allocateSyncBufferHandler();
    PerformanceCounters *getPerformanceCounters();
    PreemptionMode getPreemptionMode() const;
    bool isDebuggerActive() const;
    Debugger *getDebugger();
    SourceLevelDebugger *getSourceLevelDebugger();
    ExecutionEnvironment *getExecutionEnvironment() const;
    const RootDeviceEnvironment &getRootDeviceEnvironment() const;
    const HardwareCapabilities &getHardwareCapabilities() const;
    bool isFullRangeSvm() const;
    bool areSharedSystemAllocationsAllowed() const;
    uint32_t getRootDeviceIndex() const;
    uint32_t getNumAvailableDevices() const;

    // API entry points
    cl_int getDeviceInfo(cl_device_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet);

    bool getDeviceInfoForImage(cl_device_info paramName,
                               const void *&src,
                               size_t &srcSize,
                               size_t &retSize);

    // This helper template is meant to simplify getDeviceInfo
    template <cl_device_info Param>
    void getCap(const void *&src,
                size_t &size,
                size_t &retSize);

    template <cl_device_info Param>
    void getStr(const void *&src,
                size_t &size,
                size_t &retSize);

    Device &getDevice() const noexcept { return device; }
    const ClDeviceInfo &getDeviceInfo() const { return deviceInfo; }
    const DeviceInfo &getSharedDeviceInfo() const;
    ClDevice *getDeviceById(uint32_t deviceId);
    const std::string &peekCompilerExtensions() const;
    std::unique_ptr<SyncBufferHandler> syncBufferHandler;

  protected:
    void initializeCaps();
    void initializeExtraCaps();
    void setupFp64Flags();

    Device &device;
    std::vector<std::unique_ptr<ClDevice>> subDevices;
    cl_platform_id platformId;

    std::string name;
    std::unique_ptr<DriverInfo> driverInfo;
    unsigned int enabledClVersion = 0u;
    std::string deviceExtensions;
    std::string exposedBuiltinKernels = "";

    ClDeviceInfo deviceInfo = {};

    std::vector<unsigned int> simultaneousInterops = {0};
    std::string compilerExtensions;
};

} // namespace NEO
