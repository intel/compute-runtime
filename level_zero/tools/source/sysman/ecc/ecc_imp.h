/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/ecc/ecc.h"
#include "level_zero/tools/source/sysman/ecc/os_ecc.h"

namespace L0 {

class EccImp : public Ecc, NEO::NonCopyableOrMovableClass {
  public:
    void init() override;
    ze_result_t deviceEccAvailable(ze_bool_t *pAvailable) override;
    ze_result_t deviceEccConfigurable(ze_bool_t *pConfigurable) override;
    ze_result_t getEccState(zes_device_ecc_properties_t *pState) override;
    ze_result_t setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) override;
    OsEcc *pOsEcc = nullptr;

    EccImp(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~EccImp() override;

  private:
    OsSysman *pOsSysman = nullptr;
};

} // namespace L0
