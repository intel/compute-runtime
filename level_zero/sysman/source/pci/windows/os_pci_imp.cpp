/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/pci/windows/os_pci_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmPciImp::getProperties(zes_pci_properties_t *properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPciImp::getPciBdf(zes_pci_properties_t &pciProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void WddmPciImp::getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) {}

ze_result_t WddmPciImp::getState(zes_pci_state_t *state) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmPciImp::resizableBarSupported() {
    return false;
}

bool WddmPciImp::resizableBarEnabled(uint32_t barIndex) {
    return false;
}

ze_result_t WddmPciImp::initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmPciImp::WddmPciImp(OsSysman *pOsSysman) {}

OsPci *OsPci::create(OsSysman *pOsSysman) {
    WddmPciImp *pWddmPciImp = new WddmPciImp(pOsSysman);
    return static_cast<OsPci *>(pWddmPciImp);
}

} // namespace Sysman
} // namespace L0
