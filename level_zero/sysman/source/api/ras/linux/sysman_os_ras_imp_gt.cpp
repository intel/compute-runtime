/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/api/ras/linux/sysman_os_ras_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <cstring>
#include <sstream>

namespace L0 {
namespace Sysman {

LinuxRasSourceGt::~LinuxRasSourceGt() {
}

void LinuxRasSourceGt::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    auto rasUtilInterface = pSysmanProductHelper->getGtRasUtilInterface();
    switch (rasUtilInterface) {
    case RasInterfaceType::pmu:
        PmuRasUtil::getSupportedRasErrorTypes(errorType, pLinuxSysmanImp, isSubDevice, subDeviceId);
        break;
    default:
        break;
    }
}

ze_result_t LinuxRasSourceGt::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    return pRasUtil->rasGetState(state, clear);
}

ze_result_t LinuxRasSourceGt::osRasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) {
    return pRasUtil->rasGetStateExp(numCategoriesRequested, pState);
}

ze_result_t LinuxRasSourceGt::osRasClearStateExp(zes_ras_error_category_exp_t category) {
    return pRasUtil->rasClearStateExp(category);
}

uint32_t LinuxRasSourceGt::osRasGetCategoryCount() {
    return pRasUtil->rasGetCategoryCount();
}

LinuxRasSourceGt::LinuxRasSourceGt(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pRasUtil = RasUtil::create(pSysmanProductHelper->getGtRasUtilInterface(), pLinuxSysmanImp, type, onSubdevice, subdeviceId);
    UNRECOVERABLE_IF(pRasUtil == nullptr);
}

} // namespace Sysman
} // namespace L0