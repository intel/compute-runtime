/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ecc/ecc_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {
class LinuxSysmanImp;
class FirmwareUtil;

FirmwareUtil *EccImp::getFirmwareUtilInterface(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    return pLinuxSysmanImp->getFwUtilInterface();
}
} // namespace L0