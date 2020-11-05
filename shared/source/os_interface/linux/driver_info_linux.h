/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/driver_info.h"

namespace NEO {

class DriverInfoLinux : public DriverInfo {
  public:
    DriverInfoLinux(bool imageSupport, const PhysicalDevicePciBusInfo &pciBusInfo);
    bool getImageSupport() override;

  protected:
    bool imageSupport = true;
};

} // namespace NEO
