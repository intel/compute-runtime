/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/standby/sysman_os_standby.h"
#include "level_zero/sysman/source/api/standby/sysman_standby.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {

class StandbyImp : public Standby, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t standbyGetProperties(zes_standby_properties_t *pProperties) override;
    ze_result_t standbyGetMode(zes_standby_promo_mode_t *pMode) override;
    ze_result_t standbySetMode(const zes_standby_promo_mode_t mode) override;

    StandbyImp() = default;
    StandbyImp(OsSysman *pOsSysman, bool onSubdevice, uint32_t subDeviceId);
    ~StandbyImp() override;
    std::unique_ptr<OsStandby> pOsStandby = nullptr;

    void init();

  private:
    zes_standby_properties_t standbyProperties = {};
};

} // namespace Sysman
} // namespace L0
