/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/engine/os_engine.h"
#include "level_zero/sysman/source/windows/os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class WddmEngineImp : public OsEngine, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t getProperties(zes_engine_properties_t &properties) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; };
    bool isEngineModuleSupported() override { return false; };

    WddmEngineImp() = default;
    WddmEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId) {}
    ~WddmEngineImp() override = default;
};
} // namespace Sysman
} // namespace L0
