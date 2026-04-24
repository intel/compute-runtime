/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ecc/sysman_ecc_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

FirmwareUtil *EccImp::getFirmwareUtilInterface(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    if (pSysmanProductHelper->isEccConfigurationSupported()) {
        return pLinuxSysmanImp->getFwUtilInterface();
    }
    return nullptr;
}

ze_result_t EccImp::getEccAvailable(OsSysman *pOsSysman, ze_bool_t *pAvailable) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    return pLinuxSysmanImp->getSysmanProductHelper()->getEccAvailable(pLinuxSysmanImp, pAvailable);
}

ze_result_t EccImp::getEccConfigurable(OsSysman *pOsSysman, ze_bool_t *pConfigurable) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    return pLinuxSysmanImp->getSysmanProductHelper()->getEccConfigurable(pLinuxSysmanImp, pConfigurable);
}

ze_result_t EccImp::getEccState(OsSysman *pOsSysman, zes_device_ecc_properties_t *pState) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    return pLinuxSysmanImp->getSysmanProductHelper()->getEccState(pLinuxSysmanImp, pState);
}

} // namespace Sysman
} // namespace L0
