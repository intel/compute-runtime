/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/mocks/mock_sysman_env_vars.h"

extern bool sysmanUltsEnable;

using namespace NEO;

namespace L0 {
struct SysmanDevice;
struct SysmanDeviceImp;
struct OsSysman;
namespace ult {

class PublicWddmSysmanImp : public L0::WddmSysmanImp {
  public:
    using WddmSysmanImp::pFwUtilInterface;
    using WddmSysmanImp::pKmdSysManager;
};

class SysmanDeviceFixture : public DeviceFixture, public SysmanEnabledFixture {
  public:
    void SetUp() override;

    void TearDown() override;

    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicWddmSysmanImp *pWddmSysmanImp = nullptr;
};

} // namespace ult
} // namespace L0
