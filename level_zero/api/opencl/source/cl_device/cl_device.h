/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"

#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/cl_device/cl_device_info.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/core/source/device/device.h"

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_device_id> {
    typedef class ClDevice DerivedType;
};

class ClDevice : public BaseObject<_cl_device_id> {
  public:
    static const cl_ulong objectMagic = 0x8055832341AC8D08LL;

    ClDevice(cl_platform_id platform, ze_device_handle_t deviceHandle, bool isSubdevice) : platform(platform), isSubdevice(isSubdevice), deviceHandle(deviceHandle) { this->initializeCaps(); };
    ClDevice() = delete;

    cl_int getDeviceInfo(cl_device_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet);

    bool getDeviceInfoForImage(cl_device_info paramName,
                               const void *&src,
                               size_t &srcSize,
                               size_t &retSize);
    bool getDeviceInfoExtra(cl_device_info paramName,
                            ClDeviceInfoParam &param,
                            const void *&src,
                            size_t &srcSize,
                            size_t &retSize);

    template <cl_device_info param>
    void getCap(const void *&src,
                size_t &size,
                size_t &retSize);

    template <cl_device_info param>
    void getStr(const void *&src,
                size_t &size,
                size_t &retSize);

    bool getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const { return getDevice().getDeviceAndHostTimer(deviceTimestamp, hostTimestamp); }
    bool getHostTimer(uint64_t *hostTimestamp) const { return getDevice().getHostTimer(hostTimestamp); }
    Device &getDevice() const noexcept { return *getL0Object()->getNEODevice(); }
    const HardwareInfo &getHardwareInfo() const { return getDevice().getHardwareInfo(); };
    uint32_t getRootDeviceIndex() const { return getDevice().getRootDeviceIndex(); };
    double getPlatformHostTimerResolution() const { return getDevice().getPlatformHostTimerResolution(); };
    const DeviceInfo &getSharedDeviceInfo() const { return getDevice().getDeviceInfo(); };
    const ClDeviceInfo &getDeviceInfo() const { return deviceInfo; }
    bool getIsSubdevice() const { return this->isSubdevice; };

    ze_device_handle_t getL0Handle() const { return this->deviceHandle; }
    L0::Device *getL0Object() const { return L0::Device::fromHandle(this->deviceHandle); }

  protected:
    const RootDeviceEnvironment &getRootDeviceEnvironment() const { return getDevice().getRootDeviceEnvironment(); };
    uint32_t getNumGenericSubDevices() const { return getDevice().getNumGenericSubDevices(); };
    const CompilerProductHelper &getCompilerProductHelper() const { return getDevice().getCompilerProductHelper(); };
    MemoryManager *getMemoryManager() const { return getDevice().getMemoryManager(); };
    NEO::DriverInfo *getDriverInfo() { return getL0Object()->driverInfo.get(); }

    bool isPciBusInfoValid() const;
    cl_version getExtensionVersion(std::string name);

    static cl_command_queue_capabilities_intel getQueueFamilyCapabilitiesAll();
    MOCKABLE_VIRTUAL cl_command_queue_capabilities_intel getQueueFamilyCapabilities(EngineGroupType type);
    void getQueueFamilyName(char *outputName, EngineGroupType type);

    void initializeCaps();
    void initializeOsSpecificCaps();
    void initializeExtensionsWithVersion();
    void initializeOpenclCAllVersions();
    void initializeILsWithVersion();
    void initializeSpirvQueries();
    void setupFp64Flags();
    const std::string getClDeviceName() const;

    cl_platform_id platform{};

    ClDeviceInfo deviceInfo{};
    std::string name{};

    std::string deviceExtensions{};
    std::once_flag initializeExtensionsWithVersionOnce{};
    std::once_flag initializeSpirvQueriesOnce{};
    std::vector<unsigned int> simultaneousInterops{};

    bool isSubdevice = false;

    ze_device_handle_t deviceHandle = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<ClDevice>);

} // namespace LEO
} // namespace NEO
