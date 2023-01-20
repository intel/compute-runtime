/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/driver_info.h"

namespace NEO {

class DriverInfoLinux : public DriverInfo {
  public:
    static constexpr DriverInfoType driverInfoType = DriverInfoType::LINUX;

    DriverInfoLinux(bool imageSupport, const PhysicalDevicePciBusInfo &pciBusInfo);
    bool getMediaSharingSupport() override;

  protected:
    bool imageSupport = true;
};

} // namespace NEO
