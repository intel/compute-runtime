/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/ecc/os_ecc.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {

class WddmEccImp : public OsEcc {
  public:
    ze_result_t deviceEccAvailable(ze_bool_t *pAvailable) override;
    ze_result_t deviceEccConfigurable(ze_bool_t *pConfigurable) override;
    ze_result_t getEccState(zes_device_ecc_properties_t *pState) override;
    ze_result_t setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) override;

    WddmEccImp(OsSysman *pOsSysman);
    WddmEccImp() = default;
    ~WddmEccImp() override = default;
};

} // namespace L0
