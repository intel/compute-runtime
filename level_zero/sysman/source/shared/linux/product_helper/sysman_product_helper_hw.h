/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"

namespace L0 {
namespace Sysman {

template <PRODUCT_FAMILY gfxProduct>
class SysmanProductHelperHw : public SysmanProductHelper {
  public:
    static std::unique_ptr<SysmanProductHelper> create() {
        auto pSysmanProductHelper = std::unique_ptr<SysmanProductHelper>(new SysmanProductHelperHw());
        return pSysmanProductHelper;
    }

    // Frequency
    void getFrequencyStepSize(double *pStepSize) override;

    // Memory
    ze_result_t getMemoryProperties(zes_mem_properties_t *pProperties, const LinuxSysmanImp *pLinuxSysmanImp) override;
    ze_result_t getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, const LinuxSysmanImp *pLinuxSysmanImp) override;

    // Performance
    void getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) override;

    // temperature
    ze_result_t getGlobalMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) override;
    ze_result_t getGpuMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) override;
    ze_result_t getMemoryMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) override;
    bool isMemoryMaxTemperatureSupported() override;

    // Ras
    RasInterfaceType getGtRasUtilInterface() override;
    RasInterfaceType getHbmRasUtilInterface() override;

    // global ops
    bool isRepairStatusSupported() override;

    // Voltage
    void getCurrentVoltage(PlatformMonitoringTech *pPmt, double &voltage) override;

    ~SysmanProductHelperHw() override = default;

  protected:
    SysmanProductHelperHw() = default;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableSysmanProductHelper {
    EnableSysmanProductHelper() {
        auto sysmanProductHelperCreateFunction = SysmanProductHelperHw<gfxProduct>::create;
        sysmanProductHelperFactory[gfxProduct] = sysmanProductHelperCreateFunction;
    }
};

} // namespace Sysman
} // namespace L0
