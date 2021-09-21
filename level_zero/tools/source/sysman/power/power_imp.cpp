/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/power_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {

ze_result_t PowerImp::powerGetProperties(zes_power_properties_t *pProperties) {
    *pProperties = powerProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t PowerImp::powerGetEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    return pOsPower->getEnergyCounter(pEnergy);
}

ze_result_t PowerImp::powerGetLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) {
    return pOsPower->getLimits(pSustained, pBurst, pPeak);
}

ze_result_t PowerImp::powerSetLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) {
    return pOsPower->setLimits(pSustained, pBurst, pPeak);
}
ze_result_t PowerImp::powerGetEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    return pOsPower->getEnergyThreshold(pThreshold);
}

ze_result_t PowerImp::powerSetEnergyThreshold(double threshold) {
    return pOsPower->setEnergyThreshold(threshold);
}

PowerImp::PowerImp(OsSysman *pOsSysman, ze_device_handle_t handle) : deviceHandle(handle) {
    ze_device_properties_t deviceProperties = {};
    Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
    pOsPower = OsPower::create(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
    UNRECOVERABLE_IF(nullptr == pOsPower);
    init();
}

void PowerImp::init() {
    if (pOsPower->isPowerModuleSupported()) {
        pOsPower->getProperties(&powerProperties);
        this->initSuccess = true;
    }
}

PowerImp::~PowerImp() {
    if (nullptr != pOsPower) {
        delete pOsPower;
        pOsPower = nullptr;
    }
}

} // namespace L0
