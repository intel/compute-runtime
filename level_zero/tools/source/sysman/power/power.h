/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include "level_zero/core/source/device/device.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <vector>

namespace L0 {

struct OsSysman;
class Power : _zes_pwr_handle_t {
  public:
    virtual ~Power() = default;
    virtual ze_result_t powerGetProperties(zes_power_properties_t *pProperties) = 0;
    virtual ze_result_t powerGetEnergyCounter(zes_power_energy_counter_t *pEnergy) = 0;
    virtual ze_result_t powerGetLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t powerSetLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t powerGetEnergyThreshold(zes_energy_threshold_t *pThreshold) = 0;
    virtual ze_result_t powerSetEnergyThreshold(double threshold) = 0;
    virtual ze_result_t powerGetLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) = 0;
    virtual ze_result_t powerSetLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) = 0;

    static Power *fromHandle(zes_pwr_handle_t handle) {
        return static_cast<Power *>(handle);
    }
    inline zes_pwr_handle_t toHandle() { return this; }
    bool initSuccess = false;
    bool isCardPower = false;
};
struct PowerHandleContext {
    PowerHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~PowerHandleContext();

    ze_result_t init(std::vector<ze_device_handle_t> &deviceHandles, ze_device_handle_t coreDevice);
    ze_result_t powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower);
    ze_result_t powerGetCardDomain(zes_pwr_handle_t *phPower);

    void releasePowerHandles();

    OsSysman *pOsSysman = nullptr;
    std::vector<Power *> handleList = {};

    bool isPowerInitDone() {
        return powerInitDone;
    }

  private:
    void createHandle(ze_device_handle_t deviceHandle);
    std::once_flag initPowerOnce;
    void initPower();
    bool powerInitDone = false;
};

} // namespace L0
