/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

namespace L0 {

namespace PciLinkSpeeds {
constexpr double Pci2_5GigatransfersPerSecond = 2.5;
constexpr double Pci5_0GigatransfersPerSecond = 5.0;
constexpr double Pci8_0GigatransfersPerSecond = 8.0;
constexpr double Pci16_0GigatransfersPerSecond = 16.0;
constexpr double Pci32_0GigatransfersPerSecond = 32.0;
} // namespace PciLinkSpeeds

enum PciGenerations {
    PciGen1 = 1,
    PciGen2,
    PciGen3,
    PciGen4,
    PciGen5,
};

class Pci {
  public:
    virtual ~Pci(){};
    virtual ze_result_t pciStaticProperties(zes_pci_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetInitializedBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) = 0;
    virtual ze_result_t pciGetState(zes_pci_state_t *pState) = 0;

    virtual void init() = 0;
};

} // namespace L0
