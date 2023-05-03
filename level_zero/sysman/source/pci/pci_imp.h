/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/sysman/source/pci/os_pci.h"
#include "level_zero/sysman/source/pci/pci.h"

#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {

class PciImp : public L0::Sysman::Pci, NEO::NonCopyableOrMovableClass {
  public:
    void init() override;
    ze_result_t pciStaticProperties(zes_pci_properties_t *pProperties) override;
    ze_result_t pciGetInitializedBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) override;
    ze_result_t pciGetState(zes_pci_state_t *pState) override;
    void pciGetStaticFields();

    PciImp() = default;
    PciImp(L0::Sysman::OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~PciImp() override;
    L0::Sysman::OsPci *pOsPci = nullptr;

  private:
    L0::Sysman::OsSysman *pOsSysman = nullptr;
    bool resizableBarSupported = false;
    zes_pci_properties_t pciProperties = {};
    std::vector<zes_pci_bar_properties_t *> pciBarProperties = {};
    std::once_flag initPciOnce;
    void initPci();
};

} // namespace Sysman
} // namespace L0
