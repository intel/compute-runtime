/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

struct ContextImp : Context {
    ContextImp(DriverHandle *driverHandle);
    ~ContextImp() override = default;
    ze_result_t destroy() override;
    ze_result_t getStatus() override;
    DriverHandle *getDriverHandle() override;

  protected:
    DriverHandle *driverHandle = nullptr;
};

} // namespace L0
