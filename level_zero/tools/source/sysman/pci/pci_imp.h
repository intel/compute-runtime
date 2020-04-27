/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include "os_pci.h"
#include "pci.h"

#include <vector>

namespace L0 {

class PciImp : public Pci {
  public:
    void init() override;
    ze_result_t pciStaticProperties(zet_pci_properties_t *pProperties) override;
    ze_result_t pciGetInitializedBars(uint32_t *pCount, zet_pci_bar_properties_t *pProperties) override;

    PciImp() = default;
    PciImp(OsSysman *pOsSysman) : pOsSysman(pOsSysman) { pOsPci = nullptr; };
    ~PciImp() override;
    OsPci *pOsPci = nullptr;

    // Don't allow copies of the PciImp object
    PciImp(const PciImp &obj) = delete;
    PciImp &operator=(const PciImp &obj) = delete;

  private:
    OsSysman *pOsSysman = nullptr;
    zet_pci_properties_t pciProperties = {};
    std::vector<zet_pci_bar_properties_t *> pciBarProperties;
};

} // namespace L0
