/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/topology_map.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {
struct PhysicalDevicePciBusInfo;
struct PhysicalDevicePciSpeedInfo;
struct HardwareInfo;
enum class DriverModelType;
class ExecutionEnvironment;
class MemoryManager;
class OsContext;

class HwDeviceId : public NonCopyableClass {
  public:
    HwDeviceId(DriverModelType driverModel) : driverModelType(driverModel) {
    }

    virtual ~HwDeviceId() = default;

    DriverModelType getDriverModelType() const {
        return driverModelType;
    }

    template <typename DerivedType>
    DerivedType *as() {
        UNRECOVERABLE_IF(DerivedType::driverModelType != this->driverModelType);
        return static_cast<DerivedType *>(this);
    }

    template <typename DerivedType>
    DerivedType *as() const {
        UNRECOVERABLE_IF(DerivedType::driverModelType != this->driverModelType);
        return static_cast<const DerivedType *>(this);
    }

  protected:
    DriverModelType driverModelType;
};

class DriverModel : public NonCopyableClass {
  public:
    DriverModel(DriverModelType driverModelType)
        : driverModelType(driverModelType) {
    }

    virtual ~DriverModel() = default;

    template <typename DerivedType>
    DerivedType *as() {
        UNRECOVERABLE_IF(DerivedType::driverModelType != this->driverModelType);
        return static_cast<DerivedType *>(this);
    }

    template <typename DerivedType>
    DerivedType *as() const {
        UNRECOVERABLE_IF(DerivedType::driverModelType != this->driverModelType);
        return static_cast<const DerivedType *>(this);
    }

    virtual void setGmmInputArgs(void *args) = 0;

    virtual uint32_t getDeviceHandle() const = 0;

    MOCKABLE_VIRTUAL DriverModelType getDriverModelType() const {
        return driverModelType;
    }

    virtual PhysicalDevicePciBusInfo getPciBusInfo() const = 0;
    virtual PhysicalDevicePciSpeedInfo getPciSpeedInfo() const = 0;

    virtual size_t getMaxMemAllocSize() const {
        return std::numeric_limits<size_t>::max();
    }

    virtual bool isDriverAvailable() {
        return true;
    }

    bool skipResourceCleanup() const {
        return skipResourceCleanupVar;
    }

    virtual void cleanup() {}

    virtual bool isGpuHangDetected(OsContext &osContext) = 0;
    virtual const HardwareInfo *getHardwareInfo() const = 0;

    const TopologyMap &getTopologyMap() {
        return topologyMap;
    };

  protected:
    DriverModelType driverModelType;
    TopologyMap topologyMap;
    bool skipResourceCleanupVar = false;
};

class OSInterface : public NonCopyableClass {
  public:
    virtual ~OSInterface();
    DriverModel *getDriverModel() const;

    void setDriverModel(std::unique_ptr<DriverModel> driverModel);

    MOCKABLE_VIRTUAL bool isDebugAttachAvailable() const;
    MOCKABLE_VIRTUAL bool isLockablePointer(bool isLockable) const;
    MOCKABLE_VIRTUAL bool isSizeWithinThresholdForStaging(const void *ptr, size_t size) const;
    MOCKABLE_VIRTUAL uint32_t getAggregatedProcessCount() const;

    static bool osEnabled64kbPages;
    static bool are64kbPagesEnabled();
    static bool newResourceImplicitFlush;
    static bool gpuIdleImplicitFlush;
    static bool requiresSupportForWddmTrimNotification;
    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevices(ExecutionEnvironment &executionEnvironment);
    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevice(ExecutionEnvironment &executionEnvironment, std::string &osPciPath);

  protected:
    std::unique_ptr<DriverModel> driverModel = nullptr;
};

} // namespace NEO
