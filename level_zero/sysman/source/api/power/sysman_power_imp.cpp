/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/power/sysman_power_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/power/sysman_os_power.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t PowerImp::powerGetProperties(zes_power_properties_t *pProperties) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    void *pNext = pProperties->pNext;
    result = pOsPower->getProperties(pProperties);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pProperties->pNext = pNext;
    while (pNext) {
        zes_power_ext_properties_t *pExtProps = reinterpret_cast<zes_power_ext_properties_t *>(pNext);
        if (pExtProps->stype == ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES) {
            result = pOsPower->getPropertiesExt(pExtProps);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
        }
        pNext = pExtProps->pNext;
    }
    return result;
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

ze_result_t PowerImp::powerGetLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return pOsPower->getLimitsExt(pCount, pSustained);
}

ze_result_t PowerImp::powerSetLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return pOsPower->setLimitsExt(pCount, pSustained);
}

ze_result_t PowerImp::powerGetEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    return pOsPower->getEnergyThreshold(pThreshold);
}

ze_result_t PowerImp::powerSetEnergyThreshold(double threshold) {
    return pOsPower->setEnergyThreshold(threshold);
}

PowerImp::PowerImp(OsSysman *pOsSysman, ze_bool_t isSubDevice, uint32_t subDeviceId, zes_power_domain_t powerDomain) {

    pOsPower = OsPower::create(pOsSysman, isSubDevice, subDeviceId, powerDomain);
    UNRECOVERABLE_IF(nullptr == pOsPower);
    this->isCardPower = (powerDomain == ZES_POWER_DOMAIN_CARD);
    init();
}

void PowerImp::init() {
    if (pOsPower->isPowerModuleSupported()) {
        this->initSuccess = true;
    }
}

PowerImp::~PowerImp() {
    if (nullptr != pOsPower) {
        delete pOsPower;
        pOsPower = nullptr;
    }
}

} // namespace Sysman
} // namespace L0
