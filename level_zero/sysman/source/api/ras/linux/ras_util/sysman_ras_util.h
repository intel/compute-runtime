/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/sysman_const.h"
#include <level_zero/zes_api.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace L0 {
namespace Sysman {

class PmuInterface;
class LinuxSysmanImp;
class FsAccessInterface;
class SysFsAccessInterface;
class FirmwareUtil;

enum class RasInterfaceType {
    pmu = 0,
    pmt,
    gsc,
    netlink,
    none,
};

class RasUtil : public NEO::NonCopyableAndNonMovableClass {
  public:
    RasUtil() = default;
    static std::unique_ptr<RasUtil> create(RasInterfaceType rasInterface, LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ze_result_t rasGetState(zes_ras_state_t &state, ze_bool_t clear) = 0;
    virtual ze_result_t rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) = 0;
    virtual ze_result_t rasClearStateExp(zes_ras_error_category_exp_t category) = 0;
    virtual uint32_t rasGetCategoryCount() = 0;
    virtual ~RasUtil() = default;
};

class PmuRasUtil : public RasUtil {
  public:
    PmuRasUtil(zes_ras_error_type_t type, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t onSubdevice, uint32_t subdeviceId);
    ze_result_t rasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) override;
    ze_result_t rasClearStateExp(zes_ras_error_category_exp_t category) override;
    uint32_t rasGetCategoryCount() override;
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId);
    ~PmuRasUtil() override;

  protected:
    PmuInterface *pPmuInterface = nullptr;
    FsAccessInterface *pFsAccess = nullptr;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    zes_ras_error_type_t rasErrorType = {};

  private:
    std::vector<int64_t> memberFds = {};
    int64_t groupFd = -1;
    uint64_t absoluteErrorCount[maxRasErrorCategoryExpCount] = {0};
    uint32_t clearStatus = 0;
    std::map<zes_ras_error_category_exp_t, uint64_t> errorCategoryToEventCount;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    void closeFds();
    void initRasErrors(ze_bool_t clear);
    ze_result_t getConfig(
        const std::string &eventDirectory,
        const std::vector<std::string> &listOfEvents,
        const std::string &errorFileToGetConfig,
        std::string &pmuConfig);
    ze_result_t getAbsoluteErrorCount(
        std::string nameOfError,
        const std::string &errorCounterDir,
        uint64_t &errorVal);
    inline bool getAbsoluteCount(zes_ras_error_category_exp_t category) {
        return !(clearStatus & (1 << category));
    }
};

class GscRasUtil : public RasUtil {
  public:
    ze_result_t rasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
    ze_result_t rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) override;
    ze_result_t rasClearStateExp(zes_ras_error_category_exp_t category) override;
    static void getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId);
    uint32_t rasGetCategoryCount() override;
    GscRasUtil(zes_ras_error_type_t type, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId);
    ~GscRasUtil() override{};

  protected:
    ze_result_t getMemoryErrorCountFromFw(zes_ras_error_type_t rasErrorType, uint32_t subDeviceCount, uint64_t &errorCount);
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    zes_ras_error_type_t rasErrorType = {};
    FirmwareUtil *pFwInterface = nullptr;

  private:
    uint64_t errorBaseline = 0;
    uint32_t subdeviceId = 0;
    uint32_t subDeviceCount = 0;
};

class RasUtilNone : public RasUtil {
  public:
    ze_result_t rasGetState(zes_ras_state_t &state, ze_bool_t clear) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; };
    ze_result_t rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; };
    ze_result_t rasClearStateExp(zes_ras_error_category_exp_t category) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; };
    uint32_t rasGetCategoryCount() override { return 0u; };
    RasUtilNone() = default;
    ~RasUtilNone() override{};
};

} // namespace Sysman
} // namespace L0
