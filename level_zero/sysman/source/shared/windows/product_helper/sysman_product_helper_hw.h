/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"

namespace L0 {
namespace Sysman {

template <PRODUCT_FAMILY gfxProduct>
class SysmanProductHelperHw : public SysmanProductHelper {
  public:
    static std::unique_ptr<SysmanProductHelper> create() {
        auto pSysmanProductHelper = std::unique_ptr<SysmanProductHelper>(new SysmanProductHelperHw());
        return pSysmanProductHelper;
    }

    ~SysmanProductHelperHw() override = default;

    // Temperature
    ze_result_t getSensorTemperature(double *pTemperature, zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) override;

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
