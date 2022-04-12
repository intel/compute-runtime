/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ecc/ecc_imp.h"

namespace L0 {

ze_result_t EccImp::deviceEccAvailable(ze_bool_t *pAvailable) {
    return pOsEcc->deviceEccAvailable(pAvailable);
}

ze_result_t EccImp::deviceEccConfigurable(ze_bool_t *pConfigurable) {
    return pOsEcc->deviceEccConfigurable(pConfigurable);
}

ze_result_t EccImp::getEccState(zes_device_ecc_properties_t *pState) {
    return pOsEcc->getEccState(pState);
}

ze_result_t EccImp::setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) {
    return pOsEcc->setEccState(newState, pState);
}

void EccImp::init() {
    if (pOsEcc == nullptr) {
        pOsEcc = OsEcc::create(pOsSysman);
    }
}

EccImp::~EccImp() {
    if (nullptr != pOsEcc) {
        delete pOsEcc;
    }
}

} // namespace L0
