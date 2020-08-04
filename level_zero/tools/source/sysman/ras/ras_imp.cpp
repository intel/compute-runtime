/*
 * Copyright (C) 2020 Intel Corporation
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
    rasProperties.type = this->rasErrorType;
    rasProperties.onSubdevice = false;
    rasProperties.subdeviceId = 0;
    *pProperties = rasProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t RasImp::rasGetConfig(zes_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t RasImp::rasSetConfig(const zes_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t RasImp::rasGetState(const zes_ras_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void RasImp::init() {
    pOsRas->setRasErrorType(this->rasErrorType);
    isRasErrorSupported = pOsRas->isRasSupported();
}

RasImp::RasImp(OsSysman *pOsSysman, zes_ras_error_type_t type) {
    pOsRas = OsRas::create(pOsSysman);
    this->rasErrorType = type;
    init();
}

RasImp::~RasImp() {
    if (nullptr != pOsRas) {
        delete pOsRas;
    }
}

} // namespace L0
