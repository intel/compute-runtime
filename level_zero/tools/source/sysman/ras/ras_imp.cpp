/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/ras_imp.h"

namespace L0 {

ze_result_t RasImp::rasGetProperties(zet_ras_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t RasImp::rasGetConfig(zet_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t RasImp::rasSetConfig(const zet_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t RasImp::rasGetState(ze_bool_t clear, uint64_t *pTotalErrors, zet_ras_details_t *pDetails) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

RasImp::RasImp(OsSysman *pOsSysman) {
    pOsRas = OsRas::create(pOsSysman);
}

RasImp::~RasImp() {
    if (nullptr != pOsRas) {
        delete pOsRas;
    }
}

} // namespace L0
