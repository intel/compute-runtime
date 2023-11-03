/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ecc/sysman_ecc_imp.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class WddmSysmanImp;
class FirmwareUtil;

FirmwareUtil *EccImp::getFirmwareUtilInterface(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    return pWddmSysmanImp->getFwUtilInterface();
}

} // namespace Sysman
} // namespace L0
