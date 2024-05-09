/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

class Pci {
  public:
    virtual ~Pci() = default;
    virtual ze_result_t pciStaticProperties(zes_pci_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetInitializedBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetState(zes_pci_state_t *pState) = 0;
    virtual ze_result_t pciGetStats(zes_pci_stats_t *pStats) = 0;

    virtual void init() = 0;
};

} // namespace Sysman
} // namespace L0
