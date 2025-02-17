/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/ras/os_ras.h"
#include "level_zero/tools/source/sysman/ras/ras.h"

namespace L0 {

class RasImp : public Ras, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t rasGetProperties(zes_ras_properties_t *pProperties) override;
    ze_result_t rasGetConfig(zes_ras_config_t *pConfig) override;
    ze_result_t rasSetConfig(const zes_ras_config_t *pConfig) override;
    ze_result_t rasGetState(zes_ras_state_t *pConfig, ze_bool_t clear) override;
    ze_result_t rasGetStateExp(uint32_t *pCount, zes_ras_state_exp_t *pState) override;
    ze_result_t rasClearStateExp(zes_ras_error_category_exp_t category) override;

    RasImp() = default;
    RasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_device_handle_t deviceHandle);
    ~RasImp() override;

    OsRas *pOsRas = nullptr;
    void init();

  private:
    zes_ras_properties_t rasProperties = {};
    ze_device_handle_t deviceHandle = {};
};

} // namespace L0
