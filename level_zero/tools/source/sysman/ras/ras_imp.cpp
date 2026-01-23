/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/ras_imp.h"

#include "level_zero/tools/source/sysman/ras/os_ras.h"
#include "level_zero/tools/source/sysman/sysman_const.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include <cstring>

namespace L0 {

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

void RasImp::init() {
    pOsRas->osRasGetProperties(rasProperties);
}

RasImp::RasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_device_handle_t handle) : deviceHandle(handle) {
    uint32_t subdeviceId = 0;
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subdeviceId, onSubdevice, true);
    pOsRas = OsRas::create(pOsSysman, type, onSubdevice, subdeviceId);
    init();
}

RasImp::~RasImp() {
    if (nullptr != pOsRas) {
        delete pOsRas;
    }
}

} // namespace L0
