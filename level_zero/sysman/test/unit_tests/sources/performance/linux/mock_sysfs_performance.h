/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include <string>

namespace L0 {
namespace Sysman {
namespace ult {

class PublicLinuxPerformanceImp : public L0::Sysman::LinuxPerformanceImp {
  public:
    PublicLinuxPerformanceImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain) : L0::Sysman::LinuxPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain) {}
    ~PublicLinuxPerformanceImp() override = default;
    using L0::Sysman::LinuxPerformanceImp::pSysFsAccess;
    using L0::Sysman::LinuxPerformanceImp::pSysmanKmdInterface;
};

class MockSysFsAccessInterface : public L0::Sysman::SysFsAccessInterface {
  public:
    MockSysFsAccessInterface() = default;
    ~MockSysFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0