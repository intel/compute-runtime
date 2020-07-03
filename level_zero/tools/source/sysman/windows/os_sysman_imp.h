/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

class WddmSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    WddmSysmanImp(SysmanImp *pParentSysmanImp) : pParentSysmanImp(pParentSysmanImp){};
    WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) : pParentSysmanDeviceImp(pParentSysmanDeviceImp){};
    ~WddmSysmanImp() override = default;

    ze_result_t init() override;

  private:
    WddmSysmanImp() = delete;
    SysmanImp *pParentSysmanImp = nullptr;
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
};

} // namespace L0
