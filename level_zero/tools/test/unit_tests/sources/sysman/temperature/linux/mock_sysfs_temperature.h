/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"

#include "sysman/sysman_imp.h"
#include "sysman/temperature/temperature_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using ::testing::_;

namespace L0 {
namespace ult {

class PublicLinuxTemperatureImp : public L0::LinuxTemperatureImp {
  public:
    using LinuxTemperatureImp::pSysfsAccess;
    using LinuxTemperatureImp::type;
};
} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
