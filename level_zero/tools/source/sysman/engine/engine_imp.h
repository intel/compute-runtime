/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/engine/engine.h"
#include "level_zero/tools/source/sysman/engine/os_engine.h"
#include <level_zero/zet_api.h>
namespace L0 {
class EngineImp : public NEO::NonCopyableClass, public Engine {
  public:
    ze_result_t engineGetProperties(zet_engine_properties_t *pProperties) override;
    ze_result_t engineGetActivity(zet_engine_stats_t *pStats) override;

    EngineImp() = default;
    EngineImp(OsSysman *pOsSysman);
    ~EngineImp() override;

    OsEngine *pOsEngine = nullptr;
    void init();

  private:
    zet_engine_properties_t engineProperties = {};
};
} // namespace L0
