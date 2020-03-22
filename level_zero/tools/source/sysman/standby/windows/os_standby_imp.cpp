/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/standby/os_standby.h"

namespace L0 {

class WddmStandbyImp : public OsStandby {
  public:
    ze_result_t getMode(zet_standby_promo_mode_t &mode) override;
    ze_result_t setMode(zet_standby_promo_mode_t mode) override;
};

ze_result_t WddmStandbyImp::getMode(zet_standby_promo_mode_t &mode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmStandbyImp::setMode(zet_standby_promo_mode_t mode) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsStandby *OsStandby::create(OsSysman *pOsSysman) {
    WddmStandbyImp *pWddmStandbyImp = new WddmStandbyImp();
    return static_cast<OsStandby *>(pWddmStandbyImp);
}

} // namespace L0
