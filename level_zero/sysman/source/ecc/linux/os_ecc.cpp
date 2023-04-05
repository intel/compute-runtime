/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/ecc/ecc_imp.h"
#include "level_zero/sysman/source/linux/os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class LinuxSysmanImp;
class FirmwareUtil;

FirmwareUtil *EccImp::getFirmwareUtilInterface(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    return pLinuxSysmanImp->getFwUtilInterface();
}

} // namespace Sysman
} // namespace L0