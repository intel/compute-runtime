/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/ras/sysman_os_ras.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

class LinuxSysmanImp;
class FirmwareUtil;
class FsAccessInterface;
class SysFsAccessInterface;
class RasUtil;

class LinuxRasSources : NEO::NonCopyableAndNonMovableClass {
  public:
    virtual ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) = 0;
    virtual ze_result_t osRasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) = 0;
    virtual uint32_t osRasGetCategoryCount() = 0;
    virtual ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) = 0;
    virtual ~LinuxRasSources() = default;
};

class LinuxRasImp : public OsRas, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t osRasGetProperties(zes_ras_properties_t &properties) override;
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetStateExp(uint32_t *pCount, zes_ras_state_exp_t *pState) override;
    ze_result_t osRasGetConfig(zes_ras_config_t *config) override;
    ze_result_t osRasSetConfig(const zes_ras_config_t *config) override;
    ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) override;
    LinuxRasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxRasImp() = default;
    ~LinuxRasImp() override = default;

  protected:
    zes_ras_error_type_t osRasErrorType = {};
    FsAccessInterface *pFsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    std::vector<std::unique_ptr<L0::Sysman::LinuxRasSources>> rasSources = {};

  private:
    void initSources();
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    uint64_t totalThreshold = 0;
    uint64_t categoryThreshold[maxRasErrorCategoryCount] = {0};
};

class LinuxRasSourceGt : public LinuxRasSources {
  public:
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) override;
    ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) override;
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_bool_t isSubDevice, uint32_t subDeviceId);
    uint32_t osRasGetCategoryCount() override;
    LinuxRasSourceGt(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxRasSourceGt() = default;
    ~LinuxRasSourceGt() override;

  protected:
    std::unique_ptr<RasUtil> pRasUtil;
};

class LinuxRasSourceHbm : public LinuxRasSources {
  public:
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) override;
    ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) override;
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_bool_t isSubDevice, uint32_t subDeviceId);
    uint32_t osRasGetCategoryCount() override;
    LinuxRasSourceHbm(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t isSubDevice, uint32_t subdeviceId);
    LinuxRasSourceHbm() = default;
    ~LinuxRasSourceHbm() override;

  protected:
    std::unique_ptr<RasUtil> pRasUtil;
};

} // namespace Sysman
} // namespace L0
