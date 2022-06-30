/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <level_zero/zes_api.h>

#include "os_standby.h"
#include "standby.h"

namespace L0 {

class StandbyImp : public Standby, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t standbyGetProperties(zes_standby_properties_t *pProperties) override;
    ze_result_t standbyGetMode(zes_standby_promo_mode_t *pMode) override;
    ze_result_t standbySetMode(const zes_standby_promo_mode_t mode) override;

    StandbyImp() = default;
    StandbyImp(OsSysman *pOsSysman, ze_device_handle_t handle);
    ~StandbyImp() override;
    OsStandby *pOsStandby = nullptr;

    void init();

  private:
    zes_standby_properties_t standbyProperties = {};
    ze_device_handle_t deviceHandle = nullptr;
};

} // namespace L0
