/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/ras/sysman_os_ras.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {
class LinuxSysmanImp;
class LinuxRasImp : public OsRas, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t osRasGetProperties(zes_ras_properties_t &properties) override;
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetConfig(zes_ras_config_t *config) override;
    ze_result_t osRasSetConfig(const zes_ras_config_t *config) override;
    LinuxRasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxRasImp() = default;
    ~LinuxRasImp() override = default;

  protected:
    zes_ras_error_type_t osRasErrorType = {};
    FsAccess *pFsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;

  private:
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    uint64_t totalThreshold = 0;
    uint64_t categoryThreshold[maxRasErrorCategoryCount] = {0};
};

} // namespace Sysman
} // namespace L0
