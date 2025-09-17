/*
 * Copyright (C) 2023-2025 Intel Corporation
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
    bool isFrequencySetRangeSupported() override;
    zes_freq_throttle_reason_flags_t getThrottleReasons(SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysfsAccess, uint32_t subdeviceId, void *pNext) override;

    // Memory
    ze_result_t getMemoryProperties(zes_mem_properties_t *pProperties, LinuxSysmanImp *pLinuxSysmanImp, NEO::Drm *pDrm, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subDeviceId, bool isSubdevice) override;
    ze_result_t getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) override;
    void getMemoryHealthIndicator(FirmwareUtil *pFwInterface, zes_mem_health_t *health) override;
    ze_result_t getNumberOfMemoryChannels(LinuxSysmanImp *pLinuxSysmanImp, uint32_t *pNumChannels) override;

    // Performance
    void getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) override;
    bool isPerfFactorSupported() override;

    // temperature
    ze_result_t getGlobalMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) override;
    ze_result_t getGpuMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) override;
    ze_result_t getMemoryMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) override;
    bool isMemoryMaxTemperatureSupported() override;

    // Ras
    RasInterfaceType getGtRasUtilInterface() override;
    RasInterfaceType getHbmRasUtilInterface() override;

    // global ops
    bool isRepairStatusSupported() override;

    // power
    int32_t getPowerLimitValue(uint64_t value) override;
    uint64_t setPowerLimitValue(int32_t value) override;
    zes_limit_unit_t getPowerLimitUnit() override;
    bool isPowerSetLimitSupported() override;
    std::string getPackageCriticalPowerLimitFile() override;
    SysfsValueUnit getPackageCriticalPowerLimitNativeUnit() override;
    ze_result_t getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, LinuxSysmanImp *pLinuxSysmanImp, zes_power_domain_t powerDomain, uint32_t subDeviceId) override;

    // standby
    bool isStandbySupported(SysmanKmdInterface *pSysmanKmdInterface) override;

    // Firmware
    void getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) override;

    // Ecc
    bool isEccConfigurationSupported() override;

    // Device
    bool isUpstreamPortConnected() override;
    bool isZesInitSupported() override;

    // Pci
    ze_result_t getPciProperties(zes_pci_properties_t *pProperties) override;
    ze_result_t getPciStats(zes_pci_stats_t *pStats, LinuxSysmanImp *pLinuxSysmanImp) override;
    bool isPcieDowngradeSupported() override;
    int32_t maxPcieGenSupported() override;

    // Engine
    bool isAggregationOfSingleEnginesSupported() override;

    // Vf Management
    bool isVfMemoryUtilizationSupported() override;
    ze_result_t getVfLocalMemoryQuota(SysFsAccessInterface *pSysfsAccess, uint64_t &lMemQuota, const uint32_t &vfId) override;

    ~SysmanProductHelperHw() override = default;

    const std::map<std::string, std::map<std::string, uint64_t>> *getGuidToKeyOffsetMap() override;

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
