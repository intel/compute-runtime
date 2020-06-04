/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/power/os_power.h"

namespace L0 {

class WddmPowerImp : public OsPower {
  public:
    ze_result_t getEnergyCounter(uint64_t &energy) override;
    bool isPowerModuleSupported() override;
};

ze_result_t WddmPowerImp::getEnergyCounter(uint64_t &energy) {

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmPowerImp::isPowerModuleSupported() {
    return false;
}

OsPower *OsPower::create(OsSysman *pOsSysman) {
    WddmPowerImp *pWddmPowerImp = new WddmPowerImp();
    return static_cast<OsPower *>(pWddmPowerImp);
}

} // namespace L0
