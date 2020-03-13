/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/windows/os_sysman_imp.h"

namespace L0 {

ze_result_t WddmSysmanImp::init() {
    return ZE_RESULT_SUCCESS;
}

OsSysman *OsSysman::create(SysmanImp *pParentSysmanImp) {
    WddmSysmanImp *pWddmSysmanImp = new WddmSysmanImp(pParentSysmanImp);
    return static_cast<OsSysman *>(pWddmSysmanImp);
}

} // namespace L0
