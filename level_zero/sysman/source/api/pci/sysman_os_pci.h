/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <vector>

namespace L0 {
namespace Sysman {
int64_t convertPcieSpeedFromGTsToBs(double maxLinkSpeedInGt);
int32_t convertLinkSpeedToPciGen(double speed);
double convertPciGenToLinkSpeed(uint32_t gen);
struct OsSysman;
class OsPci {
  public:
    virtual ze_result_t getPciBdf(zes_pci_properties_t &pciProperties) = 0;
    virtual void getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) = 0;
    virtual ze_result_t getState(zes_pci_state_t *state) = 0;
    virtual ze_result_t getStats(zes_pci_stats_t *stats) = 0;
    virtual ze_result_t getProperties(zes_pci_properties_t *properties) = 0;
    virtual ze_result_t pciLinkSpeedUpdateExp(ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) = 0;
    virtual bool resizableBarSupported() = 0;
    virtual bool resizableBarEnabled(uint32_t barIndex) = 0;
    virtual ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) = 0;
    static OsPci *create(OsSysman *pOsSysman);
    virtual ~OsPci() = default;
    bool isPciDowngradePropertiesAvailable = false;
};

} // namespace Sysman
} // namespace L0
