/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "pci.h"

#include <mutex>
#include <vector>

namespace L0 {

class OsPci;
struct OsSysman;
class PciImp : public Pci, NEO::NonCopyableAndNonMovableClass {
  public:
    void init() override;
    ze_result_t pciStaticProperties(zes_pci_properties_t *pProperties) override;
    ze_result_t pciGetInitializedBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) override;
    ze_result_t pciGetState(zes_pci_state_t *pState) override;
    void pciGetStaticFields();

    PciImp(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~PciImp() override;
    OsPci *pOsPci = nullptr;

  private:
    OsSysman *pOsSysman = nullptr;
    bool resizableBarSupported = false;
    zes_pci_properties_t pciProperties = {};
    std::vector<zes_pci_bar_properties_t *> pciBarProperties = {};
    std::once_flag initPciOnce;
    void initPci();
};

} // namespace L0
