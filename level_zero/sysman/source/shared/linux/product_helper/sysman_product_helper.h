/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "igfxfmid.h"

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

enum class RasInterfaceType;

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

    // Memory
    virtual ze_result_t getMemoryProperties(zes_mem_properties_t *pProperties, LinuxSysmanImp *pLinuxSysmanImp, NEO::Drm *pDrm, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subDeviceId, bool isSubdevice) = 0;
    virtual ze_result_t getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, PlatformMonitoringTech *pPmt, SysmanDeviceImp *pDevice, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subdeviceId) = 0;

    // Performance
    virtual void getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) = 0;
    virtual bool isPerfFactorSupported() = 0;

    // temperature
    virtual ze_result_t getGlobalMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) = 0;
    virtual ze_result_t getGpuMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) = 0;
    virtual ze_result_t getMemoryMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) = 0;
    virtual bool isMemoryMaxTemperatureSupported() = 0;

    // Ras
    virtual RasInterfaceType getGtRasUtilInterface() = 0;
    virtual RasInterfaceType getHbmRasUtilInterface() = 0;

    // Global Operations
    virtual bool isRepairStatusSupported() = 0;

    // Voltage
    virtual void getCurrentVoltage(PlatformMonitoringTech *pPmt, double &voltage) = 0;

    // power
    virtual int32_t getPowerLimitValue(uint64_t value) = 0;
    virtual uint64_t setPowerLimitValue(int32_t value) = 0;
    virtual zes_limit_unit_t getPowerLimitUnit() = 0;

    // Diagnostics
    virtual bool isDiagnosticsSupported() = 0;

    // standby
    virtual bool isStandbySupported(SysmanKmdInterface *pSysmanKmdInterface) = 0;

    // Firmware
    virtual void getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) = 0;

    virtual ~SysmanProductHelper() = default;

  protected:
    SysmanProductHelper() = default;
};

} // namespace Sysman
} // namespace L0
