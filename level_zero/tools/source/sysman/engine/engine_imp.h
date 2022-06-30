/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/engine/engine.h"
#include "level_zero/tools/source/sysman/engine/os_engine.h"
#include <level_zero/zes_api.h>
namespace L0 {

class EngineImp : public Engine, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t engineGetProperties(zes_engine_properties_t *pProperties) override;
    ze_result_t engineGetActivity(zes_engine_stats_t *pStats) override;

    EngineImp() = default;
    EngineImp(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t subDeviceId);
    ~EngineImp() override;

    OsEngine *pOsEngine = nullptr;
    void init();

  private:
    zes_engine_properties_t engineProperties = {};
};
} // namespace L0
