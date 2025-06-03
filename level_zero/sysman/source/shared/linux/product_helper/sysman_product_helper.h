/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "neo_igfxfmid.h"

#include <map>
#include <memory>
#include <vector>

namespace NEO {
class Drm;
}

namespace L0 {
namespace Sysman {

struct SysmanDeviceImp;
class SysmanProductHelper;
class LinuxSysmanImp;
class PlatformMonitoringTech;
class SysmanKmdInterface;
class FirmwareUtil;
class SysFsAccessInterface;

enum class RasInterfaceType;
enum class SysfsValueUnit;

using SysmanProductHelperCreateFunctionType = std::unique_ptr<SysmanProductHelper> (*)();
extern SysmanProductHelperCreateFunctionType sysmanProductHelperFactory[IGFX_MAX_PRODUCT];

class SysmanProductHelper {
  public:
    static std::unique_ptr<SysmanProductHelper> create(PRODUCT_FAMILY product) {
        auto productHelperCreateFunction = sysmanProductHelperFactory[product];
        if (productHelperCreateFunction == nullptr) {
            return nullptr;
        }
        auto productHelper = productHelperCreateFunction();
        return productHelper;
    }

    // Frequency
    virtual void getFrequencyStepSize(double *pStepSize) = 0;
    virtual bool isFrequencySetRangeSupported() = 0;
    virtual zes_freq_throttle_reason_flags_t getThrottleReasons(LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) = 0;

    // Memory
    virtual ze_result_t getMemoryProperties(zes_mem_properties_t *pProperties, LinuxSysmanImp *pLinuxSysmanImp, NEO::Drm *pDrm, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subDeviceId, bool isSubdevice) = 0;
    virtual ze_result_t getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) = 0;
    virtual void getMemoryHealthIndicator(FirmwareUtil *pFwInterface, zes_mem_health_t *health) = 0;

    // Performance
    virtual void getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) = 0;
    virtual bool isPerfFactorSupported() = 0;

    // temperature
    virtual ze_result_t getGlobalMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) = 0;
    virtual ze_result_t getGpuMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) = 0;
    virtual ze_result_t getMemoryMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) = 0;
    virtual bool isMemoryMaxTemperatureSupported() = 0;

    // Ras
    virtual RasInterfaceType getGtRasUtilInterface() = 0;
    virtual RasInterfaceType getHbmRasUtilInterface() = 0;

    // Global Operations
    virtual bool isRepairStatusSupported() = 0;

    // power
    virtual int32_t getPowerLimitValue(uint64_t value) = 0;
    virtual uint64_t setPowerLimitValue(int32_t value) = 0;
    virtual zes_limit_unit_t getPowerLimitUnit() = 0;
    virtual bool isPowerSetLimitSupported() = 0;
    virtual std::string getPackageCriticalPowerLimitFile() = 0;
    virtual SysfsValueUnit getPackageCriticalPowerLimitNativeUnit() = 0;
    virtual ze_result_t getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, LinuxSysmanImp *pLinuxSysmanImp, zes_power_domain_t powerDomain, uint32_t subDeviceId) = 0;

    // standby
    virtual bool isStandbySupported(SysmanKmdInterface *pSysmanKmdInterface) = 0;

    // Firmware
    virtual void getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) = 0;
    virtual bool isLateBindingSupported() = 0;

    // Ecc
    virtual bool isEccConfigurationSupported() = 0;

    // Device
    virtual bool isUpstreamPortConnected() = 0;
    virtual bool isZesInitSupported() = 0;

    // Pci
    virtual ze_result_t getPciProperties(zes_pci_properties_t *pProperties) = 0;
    virtual ze_result_t getPciStats(zes_pci_stats_t *pStats, LinuxSysmanImp *pLinuxSysmanImp) = 0;

    // Engine
    virtual bool isAggregationOfSingleEnginesSupported() = 0;
    virtual ze_result_t getGroupEngineBusynessFromSingleEngines(LinuxSysmanImp *pLinuxSysmanImp, zes_engine_stats_t *pStats, zes_engine_group_t &engineGroup) = 0;

    // Vf Management
    virtual bool isVfMemoryUtilizationSupported() = 0;
    virtual ze_result_t getVfLocalMemoryQuota(SysFsAccessInterface *pSysfsAccess, uint64_t &lMemQuota, const uint32_t &vfId) = 0;

    virtual ~SysmanProductHelper() = default;
    virtual const std::map<std::string, std::map<std::string, uint64_t>> *getGuidToKeyOffsetMap() = 0;

  protected:
    SysmanProductHelper() = default;
};

} // namespace Sysman
} // namespace L0
