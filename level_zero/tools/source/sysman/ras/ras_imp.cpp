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

uint64_t getTotalErrors(zet_ras_details_t pDetails) {
    return (pDetails.numResets +
            pDetails.numProgrammingErrors +
            pDetails.numNonComputeErrors +
            pDetails.numComputeErrors +
            pDetails.numDriverErrors +
            pDetails.numDisplayErrors +
            pDetails.numCacheErrors);
}

ze_result_t RasImp::rasGetProperties(zet_ras_properties_t *pProperties) {
    rasProperties.type = this->rasErrorType;
    rasProperties.onSubdevice = false;
    rasProperties.subdeviceId = 0;
    *pProperties = rasProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t RasImp::rasGetConfig(zet_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t RasImp::rasSetConfig(const zet_ras_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t RasImp::rasGetState(ze_bool_t clear, uint64_t *pTotalErrors, zet_ras_details_t *pDetails) {
    zet_ras_details_t pDetailsInternal;
    memset(&pDetailsInternal, 0, sizeof(zet_ras_details_t));
    ze_result_t result = pOsRas->getCounterValues(&pDetailsInternal);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    *pTotalErrors = getTotalErrors(pDetailsInternal);
    if (pDetails != nullptr) {
        memcpy_s(pDetails, sizeof(zet_ras_details_t), &pDetailsInternal, sizeof(zet_ras_details_t));
    }
    return result;
}

void RasImp::init() {
    pOsRas->setRasErrorType(this->rasErrorType);
    isRasErrorSupported = pOsRas->isRasSupported();
}

RasImp::RasImp(OsSysman *pOsSysman, zet_ras_error_type_t type) {
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
