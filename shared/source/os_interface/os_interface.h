/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/driver_info.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

namespace NEO {
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

    DriverModelType getDriverModelType() const {
        return driverModelType;
    }

    virtual PhysicalDevicePciBusInfo getPciBusInfo() const = 0;
    virtual PhyicalDevicePciSpeedInfo getPciSpeedInfo() const = 0;

    virtual size_t getMaxMemAllocSize() const {
        return std::numeric_limits<size_t>::max();
    }

    virtual bool isDriverAvaliable() {
        return true;
    }

    bool skipResourceCleanup() const {
        return skipResourceCleanupVar;
    }

    virtual bool isGpuHangDetected(OsContext &osContext) = 0;

  protected:
    DriverModelType driverModelType;
    bool skipResourceCleanupVar = false;
};

class OSInterface : public NonCopyableClass {
  public:
    virtual ~OSInterface() = default;
    DriverModel *getDriverModel() const {
        return driverModel.get();
    };

    void setDriverModel(std::unique_ptr<DriverModel> driverModel) {
        this->driverModel = std::move(driverModel);
    };

    MOCKABLE_VIRTUAL bool isDebugAttachAvailable() const;
    static bool osEnabled64kbPages;
    static bool osEnableLocalMemory;
    static bool are64kbPagesEnabled() {
        return osEnabled64kbPages;
    }
    static bool newResourceImplicitFlush;
    static bool gpuIdleImplicitFlush;
    static bool requiresSupportForWddmTrimNotification;
    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevices(ExecutionEnvironment &executionEnvironment);
    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevice(ExecutionEnvironment &executionEnvironment, std::string &osPciPath);

  protected:
    std::unique_ptr<DriverModel> driverModel = nullptr;
};

static_assert(!std::is_move_constructible_v<NEO::OSInterface>);
static_assert(!std::is_copy_constructible_v<NEO::OSInterface>);
static_assert(!std::is_move_assignable_v<NEO::OSInterface>);
static_assert(!std::is_copy_assignable_v<NEO::OSInterface>);

} // namespace NEO
