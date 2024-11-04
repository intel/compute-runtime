/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/driver/sysman_driver.h"

#include <mutex>

namespace L0 {
namespace Sysman {

class SysmanDriverImp : public SysmanDriver {
  public:
    ze_result_t driverInit() override;

    void initialize(ze_result_t *result) override;

  protected:
    std::once_flag initDriverOnce;
    static ze_result_t initStatus;
};

} // namespace Sysman
} // namespace L0
