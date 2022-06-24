/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ecc/ecc_imp.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {
class WddmSysmanImp;
class FirmwareUtil;

FirmwareUtil *EccImp::getFirmwareUtilInterface(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    return pWddmSysmanImp->getFwUtilInterface();
}
} // namespace L0