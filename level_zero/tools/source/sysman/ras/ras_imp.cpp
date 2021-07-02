/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/ras_imp.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"

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
    memset(pState->category, 0, sizeof(pState->category));
    return pOsRas->osRasGetState(*pState, clear);
}

void RasImp::init() {
    pOsRas->osRasGetProperties(rasProperties);
}

RasImp::RasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_device_handle_t handle) : deviceHandle(handle) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
    pOsRas = OsRas::create(pOsSysman, type, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
    init();
}

RasImp::~RasImp() {
    if (nullptr != pOsRas) {
        delete pOsRas;
    }
}

} // namespace L0
