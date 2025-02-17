/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/ecc/sysman_ecc.h"
#include "level_zero/sysman/source/device/os_sysman.h"

namespace L0 {
namespace Sysman {
class FirmwareUtil;
struct OsSysman;

static constexpr uint8_t eccStateDisable = 0;
static constexpr uint8_t eccStateEnable = 1;
static constexpr uint8_t eccStateNone = 0xFF;
class EccImp : public Ecc, NEO::NonCopyableAndNonMovableClass {
  public:
    void init() override {}
    ze_result_t deviceEccAvailable(ze_bool_t *pAvailable) override;
    ze_result_t deviceEccConfigurable(ze_bool_t *pConfigurable) override;
    ze_result_t getEccState(zes_device_ecc_properties_t *pState) override;
    ze_result_t setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) override;

    EccImp(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~EccImp() override {}

  private:
    OsSysman *pOsSysman = nullptr;
    FirmwareUtil *pFwInterface = nullptr;

    zes_device_ecc_state_t getEccState(uint8_t state);
    static FirmwareUtil *getFirmwareUtilInterface(OsSysman *pOsSysman);
    ze_result_t getEccFwUtilInterface(FirmwareUtil *&pFwUtil);
};

} // namespace Sysman
} // namespace L0
