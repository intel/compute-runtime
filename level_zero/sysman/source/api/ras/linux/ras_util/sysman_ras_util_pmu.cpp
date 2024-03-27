/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace L0 {
namespace Sysman {

static const std::map<zes_ras_error_category_exp_t, std::vector<std::string>> categoryToListOfEventsUncorrectable = {
    {ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS,
     {"fatal-array-bist", "fatal-idi-parity", "fatal-l3-double",
      "fatal-l3-ecc-checker",
      "fatal-sqidi", "fatal-tlb", "fatal-l3bank"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_RESET,
     {"engine-reset"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS,
     {"eu-attention"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS,
     {"soc-fatal-psf-0", "soc-fatal-psf-1", "soc-fatal-psf-2", "soc-fatal-psf-csc-0",
      "soc-fatal-psf-csc-1", "soc-fatal-psf-csc-2", "soc-fatal-punit",
      "sgunit-fatal", "soc-nonfatal-punit", "sgunit-fatal", "sgunit-nonfatal", "gsc-nonfatal-mia-shutdown",
      "gsc-nonfatal-aon-parity", "gsc-nonfatal-rom-parity", "gsc-nonfatal-fuse-crc-check",
      "gsc-nonfatal-selfmbist", "gsc-nonfatal-fuse-pull", "gsc-nonfatal-sram-ecc", "gsc-nonfatal-glitch-det",
      "gsc-nonfatal-ucode-parity", "gsc-nonfatal-mia-int", "gsc-nonfatal-wdg-timeout"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS,
     {"fatal-fpu", "fatal-eu-grf", "fatal-sampler", "fatal-slm",
      "fatal-guc", "fatal-eu-ic", "fatal-subslice"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS,
     {"driver-object-migration", "driver-engine-other", "driver-ggtt",
      "driver-gt-interrupt", "driver-gt-other", "driver-guc-communication",
      "driver-rps"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS,
     {"soc-fatal-mdfi-east", "soc-fatal-mdfi-south", "soc-fatal-mdfi-west", "fatal-l3-fabric", "soc-fatal-cd0-mdfi"}}};

static const std::map<zes_ras_error_category_exp_t, std::vector<std::string>> categoryToListOfEventsCorrectable = {
    {ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS,
     {"correctable-l3-sng", "correctable-l3bank"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS,
     {"sgunit-correctable", "gsc-correctable-sram-ecc"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS,
     {"correctable-eu-grf", "correctable-eu-ic", "correctable-guc", "correctable-sampler", "correctable-slm", "correctable-subslice"}}};

static void closeFd(int64_t &fd) {
    if (fd != -1) {
        [[maybe_unused]] auto ret = NEO::SysCalls::close(static_cast<int>(fd));
        DEBUG_BREAK_IF(ret < 0);
        fd = -1;
    }
}

static const std::map<zes_ras_error_category_exp_t, zes_ras_error_cat_t> rasErrorCatExpToErrorCat = {
    {ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS, ZES_RAS_ERROR_CAT_CACHE_ERRORS},
    {ZES_RAS_ERROR_CATEGORY_EXP_RESET, ZES_RAS_ERROR_CAT_RESET},
    {ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS, ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS},
    {ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS, ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS},
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS, ZES_RAS_ERROR_CAT_COMPUTE_ERRORS},
    {ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS, ZES_RAS_ERROR_CAT_DRIVER_ERRORS},
    {ZES_RAS_ERROR_CATEGORY_EXP_DISPLAY_ERRORS, ZES_RAS_ERROR_CAT_DISPLAY_ERRORS}};

static ze_result_t readI915EventsDirectory(LinuxSysmanImp *pLinuxSysmanImp, std::vector<std::string> &listOfEvents, std::string *eventDirectory) {
    // To know how many errors are supported on a platform scan
    // /sys/devices/i915_0000_01_00.0/events/
    // all events are enumerated in sysfs at /sys/devices/i915_0000_01_00.0/events/
    // For above example device is in PCI slot 0000:01:00.0:
    SysFsAccessInterface *pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    const std::string deviceDir("device");
    const std::string sysDevicesDir("/sys/devices/");
    std::string bdfDir;
    ze_result_t result = pSysfsAccess->readSymLink(deviceDir, bdfDir);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read Symlink from %s and returning error:0x%x \n", __FUNCTION__, deviceDir.c_str(), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    const auto loc = bdfDir.find_last_of('/');
    auto bdf = bdfDir.substr(loc + 1);
    std::replace(bdf.begin(), bdf.end(), ':', '_');
    std::string i915DirName = "i915_" + bdf;
    std::string sysfsNode = sysDevicesDir + i915DirName + "/" + "events";
    if (eventDirectory != nullptr) {
        *eventDirectory = sysfsNode;
    }
    FsAccessInterface *pFsAccess = &pLinuxSysmanImp->getFsAccess();
    result = pFsAccess->listDirectory(sysfsNode, listOfEvents);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to list directories from %s and returning error:0x%x \n", __FUNCTION__, sysfsNode.c_str(), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_SUCCESS;
}

static uint64_t convertHexToUint64(std::string strVal) {
    auto loc = strVal.find('=');
    std::stringstream ss;
    ss << std::hex << strVal.substr(loc + 1);
    uint64_t config = 0;
    ss >> config;
    return config;
}

static bool getErrorType(std::map<zes_ras_error_category_exp_t, std::vector<std::string>> categoryToListOfEvents, std::vector<std::string> &eventList, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    // Naming convention of files containing config values for errors
    // error--<Name of error> Ex:- error--engine-reset  (config file with no subdevice)
    // error-gt<N>--<Name of error> Ex:- error-gt0--engine-reset (config file with subdevices)
    // error--<Name of error> Ex:- error--driver-object-migration  (config file for device level errors)
    std::string errorPrefix = "error--"; // prefix string of the file containing config value for pmu counters
    if (isSubDevice == true) {
        errorPrefix = "error-gt" + std::to_string(subDeviceId) + "--";
    }
    for (auto const &rasErrorCatToListOfEvents : categoryToListOfEvents) {
        for (auto const &nameOfError : rasErrorCatToListOfEvents.second) {
            std::string errorPrefixLocal = errorPrefix;
            if (nameOfError == "driver-object-migration") { // check for errors which occur at device level
                errorPrefixLocal = "error--";
            }
            if (std::find(eventList.begin(), eventList.end(), errorPrefixLocal + nameOfError) != eventList.end()) {
                return true;
            }
        }
    }
    return false;
}

void PmuRasUtil::closeFds() {
    for (auto &memberFd : memberFds) {
        closeFd(memberFd);
    }
    memberFds.clear();
    closeFd(groupFd);
}

PmuRasUtil::~PmuRasUtil() {
    closeFds();
}

uint32_t PmuRasUtil::rasGetCategoryCount() {
    if (rasErrorType == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
        return static_cast<uint32_t>(categoryToListOfEventsUncorrectable.size());
    }
    return static_cast<uint32_t>(categoryToListOfEventsCorrectable.size());
}

void PmuRasUtil::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    std::vector<std::string> listOfEvents = {};
    ze_result_t result = readI915EventsDirectory(pLinuxSysmanImp, listOfEvents, nullptr);
    if (result != ZE_RESULT_SUCCESS) {
        return;
    }
    if (getErrorType(categoryToListOfEventsCorrectable, listOfEvents, isSubDevice, subDeviceId) == true) {
        errorType.insert(ZES_RAS_ERROR_TYPE_CORRECTABLE);
    }
    if (getErrorType(categoryToListOfEventsUncorrectable, listOfEvents, isSubDevice, subDeviceId) == true) {
        errorType.insert(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    }
}

ze_result_t PmuRasUtil::rasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    if (clear == true) {
        closeFds();
        memset(state.category, 0, maxRasErrorCategoryCount * sizeof(uint64_t));
        memset(absoluteErrorCount, 0, maxRasErrorCategoryExpCount * sizeof(uint64_t));
    }
    initRasErrors(clear);
    // Iterate over all the file descriptor values present in vector which is mapped to given ras error category
    // Use the file descriptors to read pmu counters and add all the errors corresponding to the ras error category
    if (groupFd < 0) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    auto numEvents = memberFds.size() + 1;             // Add 1 to include groupFd as well.
    std::vector<std::uint64_t> data(2 + numEvents, 0); // In data[], event count starts from second index, first value gives number of events and second value is for timestamp
    if (pPmuInterface->pmuRead(static_cast<int>(groupFd), data.data(), sizeof(uint64_t) * data.size()) < 0) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    /* The data buffer retrieved after reading pmu counters is parsed to get the error count for each suberror category */
    uint64_t initialIndex = 2; // Initial index in the buffer from which the data be parsed begins
    for (auto errorCat = errorCategoryToEventCount.begin(); errorCat != errorCategoryToEventCount.end(); errorCat++) {
        auto errorCategory = rasErrorCatExpToErrorCat.find(errorCat->first);
        if (errorCategory == rasErrorCatExpToErrorCat.end()) {
            initialIndex += errorCat->second;
            continue;
        }
        uint64_t errorCount = 0;
        uint64_t j = 0;
        for (; j < errorCat->second; j++) {
            errorCount += data[initialIndex + j];
        }
        state.category[errorCat->first] = errorCount + absoluteErrorCount[errorCat->first];
        initialIndex += j;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t PmuRasUtil::rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) {
    initRasErrors(false);
    // Iterate over all the file descriptor values present in vector which is mapped to given ras error category
    // Use the file descriptors to read pmu counters and add all the errors corresponding to the ras error category
    if (groupFd < 0) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    auto numEvents = memberFds.size() + 1;             // Add 1 to include groupFd as well.
    std::vector<std::uint64_t> data(2 + numEvents, 0); // In data[], event count starts from second index, first value gives number of events and second value is for timestamp
    if (pPmuInterface->pmuRead(static_cast<int>(groupFd), data.data(), sizeof(uint64_t) * data.size()) < 0) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    /* The data buffer retrieved after reading pmu counters is parsed to get the error count for each suberror category */
    uint64_t initialIndex = 2; // Initial index in the buffer from which the data be parsed begins
    uint32_t categoryIdx = 0u;
    for (auto errorCat = errorCategoryToEventCount.begin(); (errorCat != errorCategoryToEventCount.end()) && (categoryIdx < numCategoriesRequested); errorCat++) {
        uint64_t errorCount = 0;
        uint64_t j = 0;
        for (; j < errorCat->second; j++) {
            errorCount += data[initialIndex + j];
        }
        pState[categoryIdx].category = errorCat->first;
        pState[categoryIdx].errorCounter = errorCount + absoluteErrorCount[errorCat->first];
        initialIndex += j;
        categoryIdx++;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t PmuRasUtil::rasClearStateExp(zes_ras_error_category_exp_t category) {
    ze_result_t result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    // check requested category is already initialized
    if (errorCategoryToEventCount.find(category) != errorCategoryToEventCount.end()) {
        closeFds();
        clearStatus |= (1 << category);
        absoluteErrorCount[category] = 0;
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

ze_result_t PmuRasUtil::getConfig(
    const std::string &eventDirectory,
    const std::vector<std::string> &listOfEvents,
    const std::string &errorFileToGetConfig,
    std::string &pmuConfig) {
    auto findErrorInList = std::find(listOfEvents.begin(), listOfEvents.end(), errorFileToGetConfig);
    if (findErrorInList == listOfEvents.end()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find %s from list of events and returning error:0x%x \n", __FUNCTION__, errorFileToGetConfig.c_str(), ZE_RESULT_ERROR_UNKNOWN);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return pFsAccess->read(eventDirectory + "/" + errorFileToGetConfig, pmuConfig);
}

ze_result_t PmuRasUtil::getAbsoluteErrorCount(
    std::string nameOfError,
    const std::string &errorCounterDir,
    uint64_t &errorVal) {
    std::replace(nameOfError.begin(), nameOfError.end(), '-', '_'); // replace - with _ to convert name of pmu config node to name of sysfs node
    return pSysfsAccess->read(errorCounterDir + "/" + nameOfError, errorVal);
}

void PmuRasUtil::initRasErrors(ze_bool_t clear) {
    // if already initialized
    if (groupFd >= 0) {
        return;
    }

    std::string eventDirectory;
    std::vector<std::string> listOfEvents = {};
    ze_result_t result = readI915EventsDirectory(pLinuxSysmanImp, listOfEvents, &eventDirectory);
    if (result != ZE_RESULT_SUCCESS) {
        return;
    }
    std::map<zes_ras_error_category_exp_t, std::vector<std::string>> categoryToListOfEvents;
    if (rasErrorType == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
        categoryToListOfEvents = categoryToListOfEventsCorrectable;
    }
    if (rasErrorType == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
        categoryToListOfEvents = categoryToListOfEventsUncorrectable;
    }
    std::string errorPrefix = "error--";                  // prefix string of the file containing config value for pmu counters
    std::string errorCounterDir = "gt/gt0/error_counter"; // Directory containing the sysfs nodes which in turn contains initial value of error count
    if (isSubdevice == true) {
        errorPrefix = "error-gt" + std::to_string(subdeviceId) + "--";
        errorCounterDir = "gt/gt" + std::to_string(subdeviceId) + "/error_counter";
    }
    // Following loop retrieves initial count of errors from sysfs and pmu config values for each ras error
    // PMU: error--<Name of error> Ex:- error--engine-reset (config with no subdevice)
    // PMU: error-gt<N>--<Name of error> Ex:- error-gt0--engine-reset (config with subdevices)
    // PMU: error--<Name of error> Ex:- error--driver-object-migration (config for device level errors)
    // Sysfs: card0/gt/gt0/error_counter/<Name of error> Ex:- gt/gt0/error_counter/engine_reset (sysfs with no subdevice)
    // Sysfs: card0/gt/gt<N>/error_counter/<Name of error> Ex:- gt/gt1/error_counter/engine_reset (sysfs with subdevices)
    // Sysfs: error_counter/<Name of error> Ex:- error_counter/driver_object_migration (sysfs for error which occur at device level)
    for (auto const &rasErrorCatToListOfEvents : categoryToListOfEvents) {
        uint64_t eventCount = 0;
        uint64_t errorCount = 0;
        for (auto const &nameOfError : rasErrorCatToListOfEvents.second) {
            std::string errorPrefixLocal = errorPrefix;
            std::string errorCounterDirLocal = errorCounterDir;
            if (nameOfError == "driver-object-migration") { // check for errors which occur at device level
                errorCounterDirLocal = "error_counter";
                errorPrefixLocal = "error--";
            }
            uint64_t initialErrorVal = 0;
            if ((clear == false) && (getAbsoluteCount(rasErrorCatToListOfEvents.first) == true)) {
                result = getAbsoluteErrorCount(nameOfError, errorCounterDirLocal, initialErrorVal);
                if (result != ZE_RESULT_SUCCESS) {
                    continue;
                }
            }
            std::string pmuConfig;
            result = getConfig(eventDirectory, listOfEvents, errorPrefixLocal + nameOfError, pmuConfig);
            if (result != ZE_RESULT_SUCCESS) {
                continue;
            }
            uint64_t config = convertHexToUint64(pmuConfig);
            if (groupFd == -1) {
                groupFd = pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP); // To get file descriptor of the group leader
                if (groupFd < 0) {
                    return;
                }
            } else {
                // The rest of the group members are created with subsequent calls with groupFd being set to the file descriptor of the group leader
                memberFds.push_back(pPmuInterface->pmuInterfaceOpen(config, static_cast<int>(groupFd), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP));
            }
            eventCount++;
            errorCount += initialErrorVal;
        }
        clearStatus &= ~(1 << rasErrorCatToListOfEvents.first);
        absoluteErrorCount[rasErrorCatToListOfEvents.first] = errorCount;
        errorCategoryToEventCount[rasErrorCatToListOfEvents.first] = eventCount;
    }
}

PmuRasUtil::PmuRasUtil(zes_ras_error_type_t type, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t onSubdevice, uint32_t subdeviceId) : pLinuxSysmanImp(pLinuxSysmanImp), rasErrorType(type), isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

} // namespace Sysman
} // namespace L0
