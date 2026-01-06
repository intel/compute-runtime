/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/sysman_ras_imp.h"

#include "level_zero/sysman/source/sysman_const.h"

#include <cstring>

namespace L0 {
namespace Sysman {

ze_result_t RasImp::rasGetProperties(zes_ras_properties_t *pProperties) {
    *pProperties = rasProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t RasImp::rasGetConfig(zes_ras_config_t *pConfig) {
    return pOsRas->osRasGetConfig(pConfig);
}

ze_result_t RasImp::rasSetConfig(const zes_ras_config_t *pConfig) {
    return pOsRas->osRasSetConfig(pConfig);
}

ze_result_t RasImp::rasGetState(zes_ras_state_t *pState, ze_bool_t clear) {
    memset(pState->category, 0, maxRasErrorCategoryCount * sizeof(uint64_t));
    return pOsRas->osRasGetState(*pState, clear);
}

ze_result_t RasImp::rasGetStateExp(uint32_t *pCount, zes_ras_state_exp_t *pState) {
    return pOsRas->osRasGetStateExp(pCount, pState);
}

ze_result_t RasImp::rasClearStateExp(zes_ras_error_category_exp_t category) {
    return pOsRas->osRasClearStateExp(category);
}

void RasImp::init() {
    pOsRas->osRasGetProperties(rasProperties);
}

ze_result_t RasImp::rasGetSupportedCategoriesExp(uint32_t *pCount, zes_ras_error_category_exp_t *pCategories) {
    return pOsRas->osRasGetSupportedCategoriesExp(pCount, pCategories);
}

ze_result_t RasImp::rasGetConfigExp(const uint32_t count, zes_intel_ras_config_exp_t *pConfig) {
    return pOsRas->osRasGetConfigExp(count, pConfig);
}

ze_result_t RasImp::rasSetConfigExp(const uint32_t count, const zes_intel_ras_config_exp_t *pConfig) {
    return pOsRas->osRasSetConfigExp(count, pConfig);
}

ze_result_t RasImp::rasGetStateExp(const uint32_t count, zes_intel_ras_state_exp_t *pState) {
    return pOsRas->osRasGetStateExp(count, pState);
}

RasImp::RasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    pOsRas = OsRas::create(pOsSysman, type, isSubDevice, subDeviceId);
    init();
}

RasImp::~RasImp() {
    if (nullptr != pOsRas) {
        delete pOsRas;
    }
}

} // namespace Sysman
} // namespace L0
