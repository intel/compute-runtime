/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

namespace NEO {
enum class DriverModelType;

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
} // namespace NEO
