/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/scheduler/linux/sysman_os_scheduler_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/engine_info.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

static ze_result_t readSchedulerValueFromSysfs(SysfsName schedulerSysfsName,
                                               LinuxSysmanImp *pSysmanImp,
                                               uint32_t subdeviceId,
                                               ze_bool_t getDefault,
                                               std::vector<std::string> &listOfEngines,
                                               zes_engine_type_flag_t engineType,
                                               uint64_t &readValue) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t i = 0;
    std::vector<uint64_t> readValueVec(listOfEngines.size());
    std::string path = "";
    auto pSysmanKmdInterface = pSysmanImp->getSysmanKmdInterface();
    auto engineBasePath = pSysmanKmdInterface->getEngineBasePath(subdeviceId);
    auto sysfsName = pSysmanKmdInterface->getSysfsFilePath(schedulerSysfsName, subdeviceId, false);
    for (const auto &engineName : listOfEngines) {
        if (getDefault) {
            path = engineBasePath + "/" + engineName + "/" + ".defaults/" + sysfsName;
        } else {
            path = engineBasePath + "/" + engineName + "/" + sysfsName;
        }
        result = pSysmanImp->getSysfsAccess().read(path, readValue);
        if (result == ZE_RESULT_SUCCESS) {
            pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::micro,
                                                       pSysmanKmdInterface->getNativeUnit(schedulerSysfsName),
                                                       readValue, readValue);
            readValueVec[i] = readValue;
            i++;
        } else {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read from %s and returning error:0x%x \n", __FUNCTION__, path.c_str(), result);
            return result;
        }
    }

    // For compute engines with different timeout values, use the maximum value
    if (schedulerSysfsName == SysfsName::sysfsNameSchedulerTimeout && engineType == ZES_ENGINE_TYPE_FLAG_COMPUTE) {
        readValue = *std::max_element(readValueVec.begin(), readValueVec.end());
        return result;
    }

    // check if all engines of the same type have the same scheduling param values
    if (std::adjacent_find(readValueVec.begin(), readValueVec.end(), std::not_equal_to<>()) == readValueVec.end()) {
        readValue = readValueVec[0];
        return result;
    } else {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

static ze_result_t writeSchedulerValueToSysfs(SysfsName schedulerSysfsName,
                                              LinuxSysmanImp *pSysmanImp,
                                              uint32_t subdeviceId,
                                              std::vector<std::string> &listOfEngines,
                                              zes_engine_type_flag_t engineType,
                                              uint64_t writeValue) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    auto pSysmanKmdInterface = pSysmanImp->getSysmanKmdInterface();
    auto sysfsName = pSysmanKmdInterface->getSysfsFilePath(schedulerSysfsName, subdeviceId, false);
    pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(schedulerSysfsName),
                                               SysfsValueUnit::micro, writeValue, writeValue);
    auto engineBasePath = pSysmanKmdInterface->getEngineBasePath(subdeviceId);
    for (const auto &engineName : listOfEngines) {
        auto path = engineBasePath + "/" + engineName + "/" + sysfsName;
        result = pSysmanImp->getSysfsAccess().write(path, writeValue);
        if (result != ZE_RESULT_SUCCESS) {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write into %s and returning error:0x%x \n", __FUNCTION__, path.c_str(), result);
            return result;
        }
    }
    return result;
}

ze_result_t LinuxSchedulerImp::getCurrentMode(zes_sched_mode_t *pMode) {
    uint64_t timeout = 0;
    uint64_t timeslice = 0;
    uint64_t heartbeat = 0;
    ze_result_t result = getPreemptTimeout(timeout, false);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get preempt timeout and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = getTimesliceDuration(timeslice, false);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get timeslice duration and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = getHeartbeatInterval(heartbeat, false);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get heartbeat interval and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    if (timeslice > 0) {
        *pMode = ZES_SCHED_MODE_TIMESLICE;
    } else {
        if (timeout > 0) {
            *pMode = ZES_SCHED_MODE_TIMEOUT;
        } else {
            if (heartbeat == 0) {
                *pMode = ZES_SCHED_MODE_EXCLUSIVE;
            } else {
                *pMode = ZES_SCHED_MODE_FORCE_UINT32;
                result = ZE_RESULT_ERROR_UNKNOWN;
            }
        }
    }
    return result;
}

ze_result_t LinuxSchedulerImp::setExclusiveModeImp() {
    uint64_t timeslice = 0, timeout = 0, heartbeat = 0;
    ze_result_t result = setPreemptTimeout(timeout);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set preempt timeout and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = setTimesliceDuration(timeslice);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set timeslice duration and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = setHeartbeatInterval(heartbeat);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    return result;
}

ze_result_t LinuxSchedulerImp::setExclusiveMode(ze_bool_t *pNeedReload) {

    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    if (pSysmanKmdInterface->isSettingExclusiveModeSupported() == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    *pNeedReload = false;

    zes_sched_mode_t currMode;
    ze_result_t result = getCurrentMode(&currMode);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get current mode and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    if (currMode == ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG) {
        // Unset this mode
        result = disableComputeUnitDebugMode(pNeedReload);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to disable COMPUTE_UNIT_DEBUG mode and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
    }

    return setExclusiveModeImp();
}

ze_result_t LinuxSchedulerImp::getTimeoutModeProperties(ze_bool_t getDefaults, zes_sched_timeout_properties_t *pConfig) {
    uint64_t heartbeat = 0;
    ze_result_t result = getHeartbeatInterval(heartbeat, getDefaults);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get heart beat interval and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    pConfig->watchdogTimeout = heartbeat;

    return result;
}

ze_result_t LinuxSchedulerImp::getTimesliceModeProperties(ze_bool_t getDefaults, zes_sched_timeslice_properties_t *pConfig) {
    uint64_t timeout = 0, timeslice = 0;
    ze_result_t result = getPreemptTimeout(timeout, getDefaults);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get preempt timeout and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = getTimesliceDuration(timeslice, getDefaults);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get timeslice duration and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    pConfig->interval = timeslice;
    pConfig->yieldTimeout = timeout;
    return result;
}

ze_result_t LinuxSchedulerImp::setTimeoutMode(zes_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReload) {

    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    if (pSysmanKmdInterface->isSettingTimeoutModeSupported() == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    *pNeedReload = false;
    zes_sched_mode_t currMode;
    ze_result_t result = getCurrentMode(&currMode);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get current mode and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    if (pProperties->watchdogTimeout < minTimeoutModeHeartbeat) {
        // watchdogTimeout(in usec) less than 5000 would be computed to
        // 0 milli seconds preempt timeout, and then after returning from
        // this method, we would end up in EXCLUSIVE mode
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (currMode == ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG) {
        // Unset this mode
        result = disableComputeUnitDebugMode(pNeedReload);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to disable COMPUTE_UNIT_DEBUG mode and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
    }

    result = setHeartbeatInterval(pProperties->watchdogTimeout);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set heartbeat interval and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint64_t timeout = (pProperties->watchdogTimeout) / 5;
    result = setPreemptTimeout(timeout);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set preempt timeout and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint64_t timeslice = 0;
    result = setTimesliceDuration(timeslice);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set timeslice duration and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    return result;
}

ze_result_t LinuxSchedulerImp::setTimesliceMode(zes_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReload) {
    if (pProperties->interval < minTimeoutInMicroSeconds) {
        // interval(in usec) less than 1000 would be computed to
        // 0 milli seconds interval.
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *pNeedReload = false;

    zes_sched_mode_t currMode;
    ze_result_t result = getCurrentMode(&currMode);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get current mode and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    if (currMode == ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG) {
        // Unset this mode
        result = disableComputeUnitDebugMode(pNeedReload);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to disable COMPUTE_UNIT_DEBUG mode and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
    }

    result = setPreemptTimeout(pProperties->yieldTimeout);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set preempt timeout and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = setTimesliceDuration(pProperties->interval);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set timeslice duration and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    uint64_t heartbeat = 2500 * (pProperties->interval);
    return setHeartbeatInterval(heartbeat);
}

ze_result_t LinuxSchedulerImp::getProperties(zes_sched_properties_t &schedProperties) {
    schedProperties.onSubdevice = onSubdevice;
    schedProperties.subdeviceId = subdeviceId;
    schedProperties.canControl = true;
    schedProperties.engines = this->engineType;
    schedProperties.supportedModes = (1 << ZES_SCHED_MODE_TIMEOUT) | (1 << ZES_SCHED_MODE_TIMESLICE) | (1 << ZES_SCHED_MODE_EXCLUSIVE);
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSchedulerImp::getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault) {

    return readSchedulerValueFromSysfs(SysfsName::sysfsNameSchedulerTimeout,
                                       pLinuxSysmanImp, subdeviceId, getDefault,
                                       listOfEngines, engineType, timeout);
}

ze_result_t LinuxSchedulerImp::getTimesliceDuration(uint64_t &timeslice, ze_bool_t getDefault) {

    return readSchedulerValueFromSysfs(SysfsName::sysfsNameSchedulerTimeslice,
                                       pLinuxSysmanImp, subdeviceId, getDefault,
                                       listOfEngines, engineType, timeslice);
}

ze_result_t LinuxSchedulerImp::getHeartbeatInterval(uint64_t &heartbeat, ze_bool_t getDefault) {

    return readSchedulerValueFromSysfs(SysfsName::sysfsNameSchedulerWatchDogTimeout,
                                       pLinuxSysmanImp, subdeviceId, getDefault,
                                       listOfEngines, engineType, heartbeat);
}

ze_result_t LinuxSchedulerImp::setPreemptTimeout(uint64_t timeout) {

    return writeSchedulerValueToSysfs(SysfsName::sysfsNameSchedulerTimeout,
                                      pLinuxSysmanImp, subdeviceId,
                                      listOfEngines, engineType, timeout);
}

ze_result_t LinuxSchedulerImp::setTimesliceDuration(uint64_t timeslice) {

    return writeSchedulerValueToSysfs(SysfsName::sysfsNameSchedulerTimeslice,
                                      pLinuxSysmanImp, subdeviceId,
                                      listOfEngines, engineType, timeslice);
}

ze_result_t LinuxSchedulerImp::setHeartbeatInterval(uint64_t heartbeat) {

    return writeSchedulerValueToSysfs(SysfsName::sysfsNameSchedulerWatchDogTimeout,
                                      pLinuxSysmanImp, subdeviceId,
                                      listOfEngines, engineType, heartbeat);
}

ze_result_t LinuxSchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    *pNeedReload = false;
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxSchedulerImp::isComputeUnitDebugModeEnabled() {
    return false;
}

ze_result_t LinuxSchedulerImp::disableComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    *pNeedReload = false;
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t OsScheduler::getNumEngineTypeAndInstances(
    std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines, OsSysman *pOsSysman, ze_bool_t onSubDevice, uint32_t subDeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    return pSysmanKmdInterface->getNumEngineTypeAndInstances(mapOfEngines, pLinuxSysmanImp, pSysfsAccess,
                                                             onSubDevice, subDeviceId);
}

LinuxSchedulerImp::LinuxSchedulerImp(
    OsSysman *pOsSysman, zes_engine_type_flag_t type, std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice,
    uint32_t subdeviceId) : engineType(type), onSubdevice(isSubdevice), subdeviceId(subdeviceId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    this->listOfEngines = listOfEngines;
}

std::unique_ptr<OsScheduler> OsScheduler::create(
    OsSysman *pOsSysman, zes_engine_type_flag_t type, std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<LinuxSchedulerImp> pLinuxSchedulerImp = std::make_unique<LinuxSchedulerImp>(pOsSysman, type, listOfEngines, isSubdevice, subdeviceId);
    return pLinuxSchedulerImp;
}

} // namespace Sysman
} // namespace L0
