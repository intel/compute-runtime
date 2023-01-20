/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/power_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

ze_result_t PowerImp::powerGetProperties(zes_power_properties_t *pProperties) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    void *pNext = pProperties->pNext;
    *pProperties = powerProperties;
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

PowerImp::PowerImp(OsSysman *pOsSysman, ze_device_handle_t handle) : deviceHandle(handle) {

    uint32_t subdeviceId = std::numeric_limits<uint32_t>::max();
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subdeviceId, onSubdevice, false);
    pOsPower = OsPower::create(pOsSysman, onSubdevice, subdeviceId);
    UNRECOVERABLE_IF(nullptr == pOsPower);

    init();
}

void PowerImp::init() {
    if (pOsPower->isPowerModuleSupported()) {
        pOsPower->getProperties(&powerProperties);
        this->initSuccess = true;
        this->isCardPower = powerProperties.onSubdevice ? false : true;
    }
}

PowerImp::~PowerImp() {
    if (nullptr != pOsPower) {
        delete pOsPower;
        pOsPower = nullptr;
    }
}

} // namespace L0
