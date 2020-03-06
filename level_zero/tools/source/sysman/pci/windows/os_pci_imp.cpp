/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "../os_pci.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {

class WddmPciImp : public OsPci {
  public:
    ze_result_t getPciBdf(std::string &bdf) override;
    ze_result_t getMaxLinkSpeed(double &maxLinkSpeed) override;
    ze_result_t getMaxLinkWidth(uint32_t &maxLinkwidth) override;
    ze_result_t getLinkGen(uint32_t &linkGen) override;
    ze_result_t initializeBarProperties(std::vector<zet_pci_bar_properties_t *> &pBarProperties) override;
};

ze_result_t WddmPciImp::getPciBdf(std::string &bdf) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPciImp::getMaxLinkSpeed(double &maxLinkSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPciImp::getMaxLinkWidth(uint32_t &maxLinkwidth) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPciImp::getLinkGen(uint32_t &linkGen) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPciImp::initializeBarProperties(std::vector<zet_pci_bar_properties_t *> &pBarProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsPci *OsPci::create(OsSysman *pOsSysman) {
    WddmPciImp *pWddmPciImp = new WddmPciImp();
    return static_cast<OsPci *>(pWddmPciImp);
}

} // namespace L0
