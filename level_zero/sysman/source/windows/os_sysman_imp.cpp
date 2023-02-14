/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/windows/os_sysman_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmSysmanImp::init() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

uint32_t WddmSysmanImp::getSubDeviceCount() {
    return 0;
}

WddmSysmanImp::WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) {
}

WddmSysmanImp::~WddmSysmanImp() {
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    WddmSysmanImp *pWddmSysmanImp = new WddmSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pWddmSysmanImp);
}

} // namespace Sysman
} // namespace L0
