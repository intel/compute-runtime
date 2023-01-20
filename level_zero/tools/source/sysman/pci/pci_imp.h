/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include <level_zero/zet_api.h>

#include "os_pci.h"
#include "pci.h"

#include <mutex>
#include <vector>

namespace L0 {

class PciImp : public Pci, NEO::NonCopyableOrMovableClass {
  public:
    void init() override;
    ze_result_t pciStaticProperties(zes_pci_properties_t *pProperties) override;
    ze_result_t pciGetInitializedBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) override;
    ze_result_t pciGetState(zes_pci_state_t *pState) override;
    void pciGetStaticFields();

    PciImp() = default;
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
