/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/standby/sysman_os_standby.h"

namespace L0 {
namespace Sysman {

class WddmStandbyImp : public OsStandby {
  public:
    ze_result_t getMode(zes_standby_promo_mode_t &mode) override;
    ze_result_t setMode(zes_standby_promo_mode_t mode) override;
    ze_result_t osStandbyGetProperties(zes_standby_properties_t &properties) override;
    bool isStandbySupported(void) override;
};

ze_result_t WddmStandbyImp::setMode(zes_standby_promo_mode_t mode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmStandbyImp::getMode(zes_standby_promo_mode_t &mode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmStandbyImp::osStandbyGetProperties(zes_standby_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmStandbyImp::isStandbySupported(void) {
    return false;
}

std::unique_ptr<OsStandby> OsStandby::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<WddmStandbyImp> pWddmStandbyImp = std::make_unique<WddmStandbyImp>();
    return pWddmStandbyImp;
}

} // namespace Sysman
} // namespace L0
