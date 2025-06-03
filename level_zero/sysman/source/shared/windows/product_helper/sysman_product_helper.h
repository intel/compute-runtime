/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "neo_igfxfmid.h"

#include <memory>
#include <vector>

namespace NEO {
class Drm;
}

namespace L0 {
namespace Sysman {

class SysmanProductHelper;

using SysmanProductHelperCreateFunctionType = std::unique_ptr<SysmanProductHelper> (*)();
extern SysmanProductHelperCreateFunctionType sysmanProductHelperFactory[IGFX_MAX_PRODUCT];
static const std::map<zes_power_domain_t, KmdSysman::PowerDomainsType> powerGroupToDomainTypeMap = {
    {ZES_POWER_DOMAIN_CARD, KmdSysman::PowerDomainsType::powerDomainCard},
    {ZES_POWER_DOMAIN_PACKAGE, KmdSysman::PowerDomainsType::powerDomainPackage},
};

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

    virtual ~SysmanProductHelper() = default;
    // Temperature
    virtual ze_result_t getSensorTemperature(double *pTemperature, zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) = 0;
    virtual bool isTempModuleSupported(zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) = 0;

    // Pci
    virtual ze_result_t getPciStats(zes_pci_stats_t *pStats, WddmSysmanImp *pWddmSysmanImp) = 0;
    virtual ze_result_t getPciProperties(zes_pci_properties_t *properties) = 0;

    // Memory
    virtual ze_result_t getMemoryBandWidth(zes_mem_bandwidth_t *pBandwidth, WddmSysmanImp *pWddmSysmanImp) = 0;

    // Power
    virtual ze_result_t getPowerPropertiesFromPmt(zes_power_properties_t *pProperties) = 0;
    virtual ze_result_t getPowerPropertiesExtFromPmt(zes_power_ext_properties_t *pExtPoperties, zes_power_domain_t powerDomain) = 0;
    virtual ze_result_t getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) = 0;

    // Firmware
    virtual bool isLateBindingSupported() = 0;

    // Pmt
    virtual std::map<unsigned long, std::map<std::string, uint32_t>> *getGuidToKeyOffsetMap() = 0;

    // init
    virtual bool isZesInitSupported() = 0;

  protected:
    SysmanProductHelper() = default;
};

} // namespace Sysman
} // namespace L0
