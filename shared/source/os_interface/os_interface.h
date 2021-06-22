/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/driver_info.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace NEO {
class ExecutionEnvironment;
class MemoryManager;
enum class DriverModelType { UNKNOWN,
                             WDDM,
                             DRM };

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

    virtual size_t getMaxMemAllocSize() const {
        return std::numeric_limits<size_t>::max();
    }

  protected:
    DriverModelType driverModelType;
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

  protected:
    std::unique_ptr<DriverModel> driverModel = nullptr;
};
} // namespace NEO
