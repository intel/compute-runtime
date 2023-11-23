/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "igfxfmid.h"

#include <memory>

namespace L0 {
namespace Sysman {

class SysmanProductHelper;
class LinuxSysmanImp;

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

    virtual ze_result_t getMemoryProperties(zes_mem_properties_t *pProperties, const LinuxSysmanImp *pLinuxSysmanImp) = 0;
    virtual ze_result_t getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, const LinuxSysmanImp *pLinuxSysmanImp) = 0;
    virtual ~SysmanProductHelper() = default;

  protected:
    SysmanProductHelper() = default;
};

} // namespace Sysman
} // namespace L0
