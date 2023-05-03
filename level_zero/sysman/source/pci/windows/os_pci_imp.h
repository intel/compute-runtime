/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/pci/os_pci.h"
#include "level_zero/sysman/source/windows/os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class KmdSysManager;
class WddmPciImp : public OsPci, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getPciBdf(zes_pci_properties_t &pciProperties) override;
    void getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) override;
    ze_result_t getState(zes_pci_state_t *state) override;
    ze_result_t getProperties(zes_pci_properties_t *properties) override;
    bool resizableBarSupported() override;
    bool resizableBarEnabled(uint32_t barIndex) override;
    ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) override;
    WddmPciImp(OsSysman *pOsSysman);
    WddmPciImp() = default;
    ~WddmPciImp() override = default;
};

} // namespace Sysman
} // namespace L0
