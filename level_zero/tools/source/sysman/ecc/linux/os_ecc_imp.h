/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/ecc/os_ecc.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {
class LinuxSysmanImp;
class FirmwareUtil;

class LinuxEccImp : public OsEcc, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t deviceEccAvailable(ze_bool_t *pAvailable) override;
    ze_result_t deviceEccConfigurable(ze_bool_t *pConfigurable) override;
    ze_result_t getEccState(zes_device_ecc_properties_t *pState) override;
    ze_result_t setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) override;
    LinuxEccImp() = default;
    LinuxEccImp(OsSysman *pOsSysman);
    ~LinuxEccImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;

  private:
    static constexpr uint8_t eccStateDisable = 0;
    static constexpr uint8_t eccStateEnable = 1;
    static constexpr uint8_t eccStateNone = 0xFF;

    zes_device_ecc_state_t getEccState(uint8_t state);
};

} // namespace L0
