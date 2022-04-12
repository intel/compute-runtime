/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ecc/windows/os_ecc_imp.h"

namespace L0 {

ze_result_t WddmEccImp::deviceEccAvailable(ze_bool_t *pAvailable) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmEccImp::deviceEccConfigurable(ze_bool_t *pConfigurable) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmEccImp::getEccState(zes_device_ecc_properties_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t WddmEccImp::setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmEccImp::WddmEccImp(OsSysman *pOsSysman) {
}

OsEcc *OsEcc::create(OsSysman *pOsSysman) {
    WddmEccImp *pWddmEccImp = new WddmEccImp(pOsSysman);
    return static_cast<OsEcc *>(pWddmEccImp);
}

} // namespace L0
