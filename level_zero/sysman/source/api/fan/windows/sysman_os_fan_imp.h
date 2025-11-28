/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/fan/sysman_os_fan.h"
#include "level_zero/sysman/source/shared/windows/sysman_kmd_sys.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class KmdSysManager;
class WddmFanImp : public OsFan, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_fan_properties_t *pProperties) override;
    ze_result_t getConfig(zes_fan_config_t *pConfig) override;
    ze_result_t setDefaultMode() override;
    ze_result_t setFixedSpeedMode(const zes_fan_speed_t *pSpeed) override;
    ze_result_t setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) override;
    ze_result_t getState(zes_fan_speed_units_t units, int32_t *pSpeed) override;

    WddmFanImp(OsSysman *pOsSysman, uint32_t fanIndex, bool multipleFansSupported);
    WddmFanImp() = default;
    ~WddmFanImp() override = default;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    uint32_t fanIndex = 0;
    bool multipleFansSupported = false;

  private:
    void setFanIndexForMultipleFans(std::vector<KmdSysman::RequestProperty> &vRequests);
    uint32_t getResponseOffset() const;
    ze_result_t handleDefaultMode(zes_fan_config_t *pConfig);
    ze_result_t handleTableMode(zes_fan_config_t *pConfig, uint32_t numPoints);

    int32_t maxPoints = 0;
};

} // namespace Sysman
} // namespace L0
