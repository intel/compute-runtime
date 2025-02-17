/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/ras/os_ras.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace L0 {
class FsAccess;
class SysfsAccess;
class PmuInterface;
class LinuxSysmanImp;
class LinuxRasSources;
class FirmwareUtil;
struct Device;

class LinuxRasImp : public OsRas, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t osRasGetProperties(zes_ras_properties_t &properties) override;
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetConfig(zes_ras_config_t *config) override;
    ze_result_t osRasSetConfig(const zes_ras_config_t *config) override;
    ze_result_t osRasGetStateExp(uint32_t *pCount, zes_ras_state_exp_t *pState) override;
    ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) override;
    LinuxRasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxRasImp() = default;
    ~LinuxRasImp() override = default;

  protected:
    zes_ras_error_type_t osRasErrorType = {};
    FsAccess *pFsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    std::vector<std::unique_ptr<L0::LinuxRasSources>> rasSources = {};

  private:
    void initSources();
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    uint64_t totalThreshold = 0;
    uint64_t categoryThreshold[maxRasErrorCategoryCount] = {0};
};

class LinuxRasSources : NEO::NonCopyableAndNonMovableClass {
  public:
    virtual ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) = 0;
    virtual ze_result_t osRasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) = 0;
    virtual uint32_t osRasGetCategoryCount() = 0;
    virtual ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) = 0;
    virtual ~LinuxRasSources() = default;
};

class LinuxRasSourceGt : public LinuxRasSources {
  public:
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) override;
    ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) override;
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle);
    uint32_t osRasGetCategoryCount() override;
    LinuxRasSourceGt(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxRasSourceGt() = default;
    ~LinuxRasSourceGt() override;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    zes_ras_error_type_t osRasErrorType = {};
    PmuInterface *pPmuInterface = nullptr;
    FsAccess *pFsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;

  private:
    void initRasErrors(ze_bool_t clear);
    ze_result_t getPmuConfig(
        const std::string &eventDirectory,
        const std::vector<std::string> &listOfEvents,
        const std::string &errorFileToGetConfig,
        std::string &pmuConfig);
    ze_result_t getBootUpErrorCountFromSysfs(
        std::string nameOfError,
        const std::string &errorCounterDir,
        uint64_t &errorVal);
    void closeFds();
    bool getAbsoluteCount(zes_ras_error_category_exp_t category) {
        return !(clearStatus & (1 << category));
    }
    int64_t groupFd = -1;
    std::vector<int64_t> memberFds = {};
    uint64_t initialErrorCount[maxRasErrorCategoryExpCount] = {0};
    uint32_t clearStatus = 0;
    std::map<zes_ras_error_category_exp_t, uint64_t> errorCategoryToEventCount;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
};

class LinuxRasSourceHbm : public LinuxRasSources {
  public:
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t osRasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) override;
    ze_result_t osRasClearStateExp(zes_ras_error_category_exp_t category) override;
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle);
    uint32_t osRasGetCategoryCount() override;
    LinuxRasSourceHbm(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, uint32_t subdeviceId);
    LinuxRasSourceHbm() = default;
    ~LinuxRasSourceHbm() override{};

  protected:
    ze_result_t getMemoryErrorCountFromFw(zes_ras_error_type_t rasErrorType, uint32_t subDeviceCount, uint64_t &errorCount);
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    zes_ras_error_type_t osRasErrorType = {};
    FirmwareUtil *pFwInterface = nullptr;
    Device *pDevice = nullptr;

  private:
    uint64_t errorBaseline = 0;
    uint32_t subdeviceId = 0;
    uint32_t subDeviceCount = 0;
};

} // namespace L0
