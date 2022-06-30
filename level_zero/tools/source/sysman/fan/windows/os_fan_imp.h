/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/fan/os_fan.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {

class KmdSysManager;
class WddmFanImp : public OsFan, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_fan_properties_t *pProperties) override;
    ze_result_t getConfig(zes_fan_config_t *pConfig) override;
    ze_result_t setDefaultMode() override;
    ze_result_t setFixedSpeedMode(const zes_fan_speed_t *pSpeed) override;
    ze_result_t setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) override;
    ze_result_t getState(zes_fan_speed_units_t units, int32_t *pSpeed) override;
    bool isFanModuleSupported() override;

    WddmFanImp(OsSysman *pOsSysman);
    WddmFanImp() = default;
    ~WddmFanImp() override = default;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;

  private:
    uint64_t prevTS = 0;
    uint32_t prevPulses = 0;
    int32_t maxPoints = 0;
};

} // namespace L0
