/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/os_sysman.h"

namespace L0 {
namespace Sysman {

class WddmSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~WddmSysmanImp() override;

    ze_result_t init() override;
    uint32_t getSubDeviceCount() override;
};

} // namespace Sysman
} // namespace L0
