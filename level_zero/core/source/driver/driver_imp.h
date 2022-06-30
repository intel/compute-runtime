/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver.h"

#include <mutex>
#include <string>

namespace L0 {

class DriverImp : public Driver {
  public:
    ze_result_t driverInit(ze_init_flags_t flags) override;

    void initialize(ze_result_t *result) override;

  protected:
    std::once_flag initDriverOnce;
    static ze_result_t initStatus;
};

struct L0EnvVariables {
    std::string affinityMask;
    bool programDebugging;
    bool metrics;
    bool pin;
    bool sysman;
    bool pciIdDeviceOrder;
};

} // namespace L0
