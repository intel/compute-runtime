/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/api/ras/linux/sysman_os_ras_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

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
    case RasInterfaceType::netlink:
        NetlinkRasUtil::getSupportedRasErrorTypes(errorType, pLinuxSysmanImp, isSubDevice, subDeviceId);
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

ze_result_t LinuxRasSourceGt::osRasGetStateExp2(const uint32_t categoryCount, const zes_ras_error_category_exp_t *pCategories, zes_intel_ras_state_exp2_t *pStates) {
    return pRasUtil->rasGetStateExp2(categoryCount, pCategories, pStates);
}

ze_result_t LinuxRasSourceGt::osRasClearStateExp(zes_ras_error_category_exp_t category) {
    return pRasUtil->rasClearStateExp(category);
}

uint32_t LinuxRasSourceGt::osRasGetCategoryCount() {
    return pRasUtil->rasGetCategoryCount();
}

std::vector<zes_ras_error_category_exp_t> LinuxRasSourceGt::getSupportedErrorCategoriesExp() {
    return pRasUtil->getSupportedErrorCategoriesExp();
}

ze_result_t LinuxRasSourceGt::osRasSetConfigExp(const uint32_t count, const zes_intel_ras_config_exp_t *pConfig) {
    return pRasUtil->rasSetConfigExp(count, pConfig);
}

ze_result_t LinuxRasSourceGt::osRasGetConfigExp(const uint32_t count, zes_intel_ras_config_exp_t *pConfig) {
    return pRasUtil->rasGetConfigExp(count, pConfig);
}

LinuxRasSourceGt::LinuxRasSourceGt(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pRasUtil = RasUtil::create(pSysmanProductHelper->getGtRasUtilInterface(), pLinuxSysmanImp, type, onSubdevice, subdeviceId);
    UNRECOVERABLE_IF(pRasUtil == nullptr);
}

} // namespace Sysman
} // namespace L0
