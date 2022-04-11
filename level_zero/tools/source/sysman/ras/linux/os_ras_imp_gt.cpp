/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp_prelim.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {
static const std::map<zes_ras_error_cat_t, std::vector<std::string>> categoryToListOfEventsUncorrectable = {
    {ZES_RAS_ERROR_CAT_CACHE_ERRORS,
     {"fatal-array-bist", "fatal-eu-grf", "fatal-eu-ic",
      "fatal-guc", "fatal-idi-parity", "fatal-l3-double",
      "fatal-l3-ecc-checker", "fatal-sampler", "fatal-slm",
      "fatal-sqidi", "fatal-tlb"}},
    {ZES_RAS_ERROR_CAT_RESET,
     {"engine-reset"}},
    {ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
     {"eu-attention"}},
    {ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS,
     {"soc-fatal-fabric-ss0-0", "soc-fatal-fabric-ss0-1", "soc-fatal-fabric-ss0-2",
      "soc-fatal-fabric-ss0-3", "soc-fatal-fabric-ss1-0", "soc-fatal-fabric-ss1-1",
      "soc-fatal-fabric-ss1-2", "soc-fatal-fabric-ss1-3", "soc-fatal-fabric-ss1-4",
      "soc-fatal-hbm-ss0-0", "soc-fatal-hbm-ss0-1", "soc-fatal-hbm-ss0-2",
      "soc-fatal-hbm-ss0-3", "soc-fatal-hbm-ss0-4", "soc-fatal-hbm-ss0-5",
      "soc-fatal-hbm-ss0-6", "soc-fatal-hbm-ss0-7", "soc-fatal-hbm-ss0-8",
      "soc-fatal-hbm-ss0-9", "soc-fatal-hbm-ss0-10", "soc-fatal-hbm-ss0-11",
      "soc-fatal-hbm-ss0-12", "soc-fatal-hbm-ss0-13", "soc-fatal-hbm-ss0-14",
      "soc-fatal-hbm-ss0-15", "soc-fatal-hbm-ss1-0", "soc-fatal-hbm-ss1-1",
      "soc-fatal-hbm-ss1-2", "soc-fatal-hbm-ss1-3", "soc-fatal-hbm-ss1-4",
      "soc-fatal-hbm-ss1-5", "soc-fatal-hbm-ss1-6", "soc-fatal-hbm-ss1-7",
      "soc-fatal-hbm-ss1-8", "soc-fatal-hbm-ss1-9", "soc-fatal-hbm-ss1-10",
      "soc-fatal-hbm-ss1-11", "soc-fatal-hbm-ss1-12", "soc-fatal-hbm-ss1-13",
      "soc-fatal-hbm-ss1-14", "soc-fatal-hbm-ss1-15", "soc-fatal-mdfi-east",
      "soc-fatal-mdfi-south", "soc-fatal-mdfi-west", "soc-fatal-psf-csc-0",
      "soc-fatal-psf-csc-1", "soc-fatal-psf-csc-2", "soc-fatal-punit",
      "sgunit-fatal", "soc-nonfatal-fabric-ss0-0", "soc-nonfatal-fabric-ss0-1",
      "soc-nonfatal-fabric-ss0-2", "soc-nonfatal-fabric-ss0-3", "soc-nonfatal-fabric-ss1-0",
      "soc-nonfatal-fabric-ss1-1", "soc-nonfatal-fabric-ss1-2", "soc-nonfatal-fabric-ss1-3",
      "soc-nonfatal-fabric-ss1-4", "soc-nonfatal-hbm-ss0-0", "soc-nonfatal-hbm-ss0-1",
      "soc-nonfatal-hbm-ss0-2", "soc-nonfatal-hbm-ss0-3", "soc-nonfatal-hbm-ss0-4",
      "soc-nonfatal-hbm-ss0-5", "soc-nonfatal-hbm-ss0-6", "soc-nonfatal-hbm-ss0-7",
      "soc-nonfatal-hbm-ss0-8", "soc-nonfatal-hbm-ss0-9", "soc-nonfatal-hbm-ss0-10",
      "soc-nonfatal-hbm-ss0-11", "soc-nonfatal-hbm-ss0-12", "soc-nonfatal-hbm-ss0-13",
      "soc-nonfatal-hbm-ss0-14", "soc-nonfatal-hbm-ss0-15", "soc-nonfatal-hbm-ss1-0",
      "soc-nonfatal-hbm-ss1-1", "soc-nonfatal-hbm-ss1-2", "soc-nonfatal-hbm-ss1-3",
      "soc-nonfatal-hbm-ss1-4", "soc-nonfatal-hbm-ss1-5", "soc-nonfatal-hbm-ss1-6",
      "soc-nonfatal-hbm-ss1-7", "soc-nonfatal-hbm-ss1-8", "soc-nonfatal-hbm-ss1-9",
      "soc-nonfatal-hbm-ss1-10", "soc-nonfatal-hbm-ss1-11", "soc-nonfatal-hbm-ss1-12",
      "soc-nonfatal-hbm-ss1-13", "soc-nonfatal-hbm-ss1-14", "soc-nonfatal-hbm-ss1-15",
      "soc-nonfatal-mdfi-east", "soc-nonfatal-mdfi-south", "soc-nonfatal-mdfi-west",
      "soc-nonfatal-psf-csc-0", "soc-nonfatal-psf-csc-1", "soc-nonfatal-psf-csc-2",
      "soc-nonfatal-punit", "sgunit-nonfatal"}},
    {ZES_RAS_ERROR_CAT_COMPUTE_ERRORS,
     {"fatal-fpu", "fatal-l3-fabric"}},
    {ZES_RAS_ERROR_CAT_DRIVER_ERRORS,
     {"driver-object-migration", "driver-engine-other", "driver-ggtt",
      "driver-gt-interrupt", "driver-gt-other", "driver-guc-communication",
      "driver-rps"}}};

static const std::map<zes_ras_error_cat_t, std::vector<std::string>> categoryToListOfEventsCorrectable = {
    {ZES_RAS_ERROR_CAT_CACHE_ERRORS,
     {"correctable-eu-grf", "correctable-eu-ic", "correctable-guc",
      "correctable-l3-sng", "correctable-sampler", "correctable-slm"}},
    {ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS,
     {"soc-correctable-fabric-ss0-0", "soc-correctable-fabric-ss0-1", "soc-correctable-fabric-ss0-2",
      "soc-correctable-fabric-ss0-3", "soc-correctable-fabric-ss1-0", "soc-correctable-fabric-ss1-1",
      "soc-correctable-fabric-ss1-2", "soc-correctable-fabric-ss1-3", "soc-correctable-fabric-ss1-4",
      "soc-correctable-hbm-ss0-0", "soc-correctable-hbm-ss0-1", "soc-correctable-hbm-ss0-2",
      "soc-correctable-hbm-ss0-3", "soc-correctable-hbm-ss0-4", "soc-correctable-hbm-ss0-5",
      "soc-correctable-hbm-ss0-6", "soc-correctable-hbm-ss0-7", "soc-correctable-hbm-ss0-8",
      "soc-correctable-hbm-ss0-9", "soc-correctable-hbm-ss0-10", "soc-correctable-hbm-ss0-11",
      "soc-correctable-hbm-ss0-12", "soc-correctable-hbm-ss0-13", "soc-correctable-hbm-ss0-14",
      "soc-correctable-hbm-ss0-15", "soc-correctable-hbm-ss1-0", "soc-correctable-hbm-ss1-1",
      "soc-correctable-hbm-ss1-2", "soc-correctable-hbm-ss1-3", "soc-correctable-hbm-ss1-4",
      "soc-correctable-hbm-ss1-5", "soc-correctable-hbm-ss1-6", "soc-correctable-hbm-ss1-7",
      "soc-correctable-hbm-ss1-8", "soc-correctable-hbm-ss1-9", "soc-correctable-hbm-ss1-10",
      "soc-correctable-hbm-ss1-11", "soc-correctable-hbm-ss1-12", "soc-correctable-hbm-ss1-13",
      "soc-correctable-hbm-ss1-14", "soc-correctable-hbm-ss1-15", "soc-correctable-mdfi-east",
      "soc-correctable-mdfi-south", "soc-correctable-mdfi-west", "soc-correctable-psf-csc-0",
      "soc-correctable-psf-csc-1", "soc-correctable-punit", "sgunit-correctable"}}};

static void closeFd(int64_t &fd) {
    if (fd != -1) {
        close(static_cast<int>(fd));
        fd = -1;
    }
}

static ze_result_t readI915EventsDirectory(LinuxSysmanImp *pLinuxSysmanImp, std::vector<std::string> &listOfEvents, std::string *eventDirectory) {
    // To know how many errors are supported on a platform scan
    // /sys/devices/i915_0000_01_00.0/events/
    // all events are enumerated in sysfs at /sys/devices/i915_0000_01_00.0/events/
    // For above example device is in PCI slot 0000:01:00.0:
    SysfsAccess *pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    const std::string deviceDir("device");
    const std::string sysDevicesDir("/sys/devices/");
    std::string bdfDir;
    ze_result_t result = pSysfsAccess->readSymLink(deviceDir, bdfDir);
    if (ZE_RESULT_SUCCESS != result) {
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
    FsAccess *pFsAccess = &pLinuxSysmanImp->getFsAccess();
    result = pFsAccess->listDirectory(sysfsNode, listOfEvents);
    if (ZE_RESULT_SUCCESS != result) {
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

static bool getErrorType(std::map<zes_ras_error_cat_t, std::vector<std::string>> categoryToListOfEvents, std::vector<std::string> &eventList, ze_device_handle_t deviceHandle) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
    bool onSubDevice = deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
    uint32_t subDeviceId = deviceProperties.subdeviceId;
    // Naming convention of files containing config values for errors
    // error--<Name of error> Ex:- error--engine-reset  (config file with no subdevice)
    // error-gt<N>--<Name of error> Ex:- error-gt0--engine-reset (config file with subdevices)
    // error--<Name of error> Ex:- error--driver-object-migration  (config file for device level errors)
    std::string errorPrefix = "error--"; // prefix string of the file containing config value for pmu counters
    if (onSubDevice == true) {
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

void LinuxRasSourceGt::closeFds() {
    for (auto &memberFd : memberFds) {
        closeFd(memberFd);
    }
    memberFds.clear();
    closeFd(groupFd);
}

LinuxRasSourceGt::~LinuxRasSourceGt() {
    closeFds();
}

void LinuxRasSourceGt::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    std::vector<std::string> listOfEvents = {};
    ze_result_t result = readI915EventsDirectory(pLinuxSysmanImp, listOfEvents, nullptr);
    if (result != ZE_RESULT_SUCCESS) {
        return;
    }
    if (getErrorType(categoryToListOfEventsCorrectable, listOfEvents, deviceHandle) == true) {
        errorType.insert(ZES_RAS_ERROR_TYPE_CORRECTABLE);
    }
    if (getErrorType(categoryToListOfEventsUncorrectable, listOfEvents, deviceHandle) == true) {
        errorType.insert(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    }
}

ze_result_t LinuxRasSourceGt::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    if (clear == true) {
        closeFds();
        totalEventCount = 0;
        memset(state.category, 0, sizeof(state.category));
        memset(initialErrorCount, 0, sizeof(initialErrorCount));
    }
    initRasErrors(clear);
    // Iterate over all the file descriptor values present in vector which is mapped to given ras error category
    // Use the file descriptors to read pmu counters and add all the errors corresponding to the ras error category
    if (groupFd < 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<zes_ras_error_cat_t, std::vector<std::string>> categoryToEvent;
    if (osRasErrorType == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
        categoryToEvent = categoryToListOfEventsCorrectable;
    }
    if (osRasErrorType == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
        categoryToEvent = categoryToListOfEventsUncorrectable;
    }
    std::vector<std::uint64_t> data(2 + totalEventCount, 0); // In data[], event count starts from second index, first value gives number of events and second value is for timestamp
    if (pPmuInterface->pmuRead(static_cast<int>(groupFd), data.data(), sizeof(uint64_t) * data.size()) < 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    /* The data buffer retrieved after reading pmu counters is parsed to get the error count for each suberror category */
    uint64_t initialIndex = 2; // Initial index in the buffer from which the data be parsed begins
    for (auto errorCat = errorCategoryToEventCount.begin(); errorCat != errorCategoryToEventCount.end(); errorCat++) {
        uint64_t errorCount = 0;
        uint64_t j = 0;
        for (; j < errorCat->second; j++) {
            errorCount += data[initialIndex + j];
        }
        state.category[errorCat->first] = errorCount + initialErrorCount[errorCat->first];
        initialIndex += j;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxRasSourceGt::getPmuConfig(
    const std::string &eventDirectory,
    const std::vector<std::string> &listOfEvents,
    const std::string &errorFileToGetConfig,
    std::string &pmuConfig) {
    auto findErrorInList = std::find(listOfEvents.begin(), listOfEvents.end(), errorFileToGetConfig);
    if (findErrorInList == listOfEvents.end()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return pFsAccess->read(eventDirectory + "/" + errorFileToGetConfig, pmuConfig);
}

ze_result_t LinuxRasSourceGt::getBootUpErrorCountFromSysfs(
    std::string nameOfError,
    const std::string &errorCounterDir,
    uint64_t &errorVal) {
    std::replace(nameOfError.begin(), nameOfError.end(), '-', '_'); // replace - with _ to convert name of pmu config node to name of sysfs node
    return pSysfsAccess->read(errorCounterDir + "/" + nameOfError, errorVal);
}

void LinuxRasSourceGt::initRasErrors(ze_bool_t clear) {

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
    std::map<zes_ras_error_cat_t, std::vector<std::string>> categoryToListOfEvents;
    if (osRasErrorType == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
        categoryToListOfEvents = categoryToListOfEventsCorrectable;
    }
    if (osRasErrorType == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
        categoryToListOfEvents = categoryToListOfEventsUncorrectable;
    }
    std::string errorPrefix = "error--";                  // prefix string of the file containing config value for pmu counters
    std::string errorCounterDir = "gt/gt0/error_counter"; // Directory containing the sysfs nodes which in turn contains initial value of error count
    if (isSubdevice == true) {
        errorPrefix = "error-gt" + std::to_string(subdeviceId) + "--";
        errorCounterDir = "gt/gt" + std::to_string(subdeviceId) + "/error_counter";
    }
    // Following loop retrieves initial count of errors from sysfs and pmu config values for each ras error
    // PMU: error--<Name of error> Ex:- error--engine-reset  (config with no subdevice)
    // PMU: error-gt<N>--<Name of error> Ex:- error-gt0--engine-reset (config with subdevices)
    // PMU: error--<Name of error> Ex:- error--driver-object-migration  (config for device level errors)
    // Sysfs: card0/gt/gt0/error_counter/<Name of error> Ex:- gt/gt0/error_counter/engine_reset  (sysfs with no subdevice)
    // Sysfs: card0/gt/gt<N>/error_counter/<Name of error> Ex:- gt/gt1/error_counter/engine_reset  (sysfs with dubdevices)
    // Sysfs: error_counter/<Name of error> Ex:- error_counter/driver_object_migration  (sysfs for error which occur at device level)
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
            if (clear == false) {
                result = getBootUpErrorCountFromSysfs(nameOfError, errorCounterDirLocal, initialErrorVal);
                if (result != ZE_RESULT_SUCCESS) {
                    continue;
                }
            }
            std::string pmuConfig;
            result = getPmuConfig(eventDirectory, listOfEvents, errorPrefixLocal + nameOfError, pmuConfig);
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
        initialErrorCount[rasErrorCatToListOfEvents.first] = errorCount;
        errorCategoryToEventCount[rasErrorCatToListOfEvents.first] = eventCount;
        totalEventCount += eventCount;
    }
}

LinuxRasSourceGt::LinuxRasSourceGt(LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) : pLinuxSysmanImp(pLinuxSysmanImp), osRasErrorType(type), isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

} // namespace L0