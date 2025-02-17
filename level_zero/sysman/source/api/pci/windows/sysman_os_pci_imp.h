/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/pci/sysman_os_pci.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {
struct OsSysman;
class KmdSysManager;
class WddmPciImp : public OsPci, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getPciBdf(zes_pci_properties_t &pciProperties) override;
    void getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) override;
    ze_result_t getState(zes_pci_state_t *state) override;
    ze_result_t getStats(zes_pci_stats_t *stats) override;
    ze_result_t getProperties(zes_pci_properties_t *properties) override;
    bool resizableBarSupported() override;
    bool resizableBarEnabled(uint32_t barIndex) override;
    ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) override;
    WddmPciImp(OsSysman *pOsSysman);
    ~WddmPciImp() override = default;
    bool isLocalMemSupported();

  protected:
    WddmSysmanImp *pWddmSysmanImp = nullptr;
    KmdSysManager *pKmdSysManager = nullptr;

  private:
    bool isLmemSupported = false;
};

} // namespace Sysman
} // namespace L0
