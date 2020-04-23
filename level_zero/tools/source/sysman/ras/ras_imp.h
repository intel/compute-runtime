/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/ras/os_ras.h"
#include "level_zero/tools/source/sysman/ras/ras.h"

namespace L0 {

class RasImp : public NEO::NonCopyableClass, public Ras {
  public:
    ze_result_t rasGetProperties(zet_ras_properties_t *pProperties) override;
    ze_result_t rasGetConfig(zet_ras_config_t *pConfig) override;
    ze_result_t rasSetConfig(const zet_ras_config_t *pConfig) override;
    ze_result_t rasGetState(ze_bool_t clear, uint64_t *pTotalErrors, zet_ras_details_t *pDetails) override;

    RasImp(OsSysman *pOsSysman);
    ~RasImp() override;

  private:
    OsRas *pOsRas = nullptr;
    void init();
    zet_ras_properties_t rasProperties = {};
};

} // namespace L0
