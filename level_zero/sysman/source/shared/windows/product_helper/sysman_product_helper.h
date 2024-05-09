/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"
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

class SysmanProductHelper;

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

    virtual ~SysmanProductHelper() = default;
    // Temperature
    virtual ze_result_t getSensorTemperature(double *pTemperature, zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) = 0;

    // Pci
    virtual ze_result_t getPciStats(zes_pci_stats_t *pStats, WddmSysmanImp *pWddmSysmanImp) = 0;

  protected:
    SysmanProductHelper() = default;
};

} // namespace Sysman
} // namespace L0
