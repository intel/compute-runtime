/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/engine/sysman_engine.h"
#include "level_zero/sysman/source/api/engine/sysman_os_engine.h"
#include <level_zero/zes_api.h>
namespace L0 {
namespace Sysman {

class EngineImp : public Engine, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t engineGetProperties(zes_engine_properties_t *pProperties) override;
    ze_result_t engineGetActivity(zes_engine_stats_t *pStats) override;
    ze_result_t engineGetActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) override;

    EngineImp() = default;
    EngineImp(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubdevice);
    ~EngineImp() override;

    std::unique_ptr<OsEngine> pOsEngine;
    void init();

  private:
    zes_engine_properties_t engineProperties = {};
};
} // namespace Sysman
} // namespace L0
