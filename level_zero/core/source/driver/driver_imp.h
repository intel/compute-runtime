/*
 * Copyright (C) 2019-2020 Intel Corporation
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
    ze_result_t driverInit(_ze_init_flag_t) override;

    void initialize(bool *result) override;

  protected:
    std::once_flag initDriverOnce;
    static bool initStatus;
};

struct L0EnvVariables {
    std::string affinityMask;
    bool programDebugging;
};

} // namespace L0
