/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {
class MockSysmanKmdInterfaceXe : public L0::Sysman::SysmanKmdInterfaceXe {

  public:
    using L0::Sysman::SysmanKmdInterface::pFsAccess;
    using L0::Sysman::SysmanKmdInterface::pProcfsAccess;
    using L0::Sysman::SysmanKmdInterface::pSysfsAccess;
    MockSysmanKmdInterfaceXe(SysmanProductHelper *pSysmanProductHelper) : SysmanKmdInterfaceXe(pSysmanProductHelper) {}
    ~MockSysmanKmdInterfaceXe() override = default;

    ADDMETHOD_NOBASE(getEngineActivityFd, int64_t, -1, (zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pPmuInterface));
};

} // namespace ult
} // namespace Sysman
} // namespace L0
