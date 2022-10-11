/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/scheduler/linux/os_scheduler_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const std::string LinuxSchedulerImp::preemptTimeoutMilliSecs("preempt_timeout_ms");
const std::string LinuxSchedulerImp::defaultPreemptTimeouttMilliSecs(".defaults/preempt_timeout_ms");
const std::string LinuxSchedulerImp::timesliceDurationMilliSecs("timeslice_duration_ms");
const std::string LinuxSchedulerImp::defaultTimesliceDurationMilliSecs(".defaults/timeslice_duration_ms");
const std::string LinuxSchedulerImp::heartbeatIntervalMilliSecs("heartbeat_interval_ms");
const std::string LinuxSchedulerImp::defaultHeartbeatIntervalMilliSecs(".defaults/heartbeat_interval_ms");
const std::string LinuxSchedulerImp::enableEuDebug("");
const std::string LinuxSchedulerImp::engineDir("engine");

ze_result_t LinuxSchedulerImp::getProperties(zes_sched_properties_t &schedProperties) {
    schedProperties.onSubdevice = onSubdevice;
    schedProperties.subdeviceId = subdeviceId;
    schedProperties.canControl = canControlScheduler();
    schedProperties.engines = this->engineType;
    schedProperties.supportedModes = (1 << ZES_SCHED_MODE_TIMEOUT) | (1 << ZES_SCHED_MODE_TIMESLICE) | (1 << ZES_SCHED_MODE_EXCLUSIVE);
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSchedulerImp::getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t i = 0;
    std::vector<uint64_t> timeoutVec = {};
    timeoutVec.resize(listOfEngines.size());
    for (const auto &engineName : listOfEngines) {
        if (getDefault) {
            result = pSysfsAccess->read(engineDir + "/" + engineName + "/" + defaultPreemptTimeouttMilliSecs, timeout);
        } else {
            result = pSysfsAccess->read(engineDir + "/" + engineName + "/" + preemptTimeoutMilliSecs, timeout);
        }
        if (result == ZE_RESULT_SUCCESS) {
            timeout = timeout * milliSecsToMicroSecs;
            timeoutVec[i] = timeout;
            i++;
        } else {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            return result;
        }
    }
    // check if all engines of the same type have the same scheduling param values
    if (std::adjacent_find(timeoutVec.begin(), timeoutVec.end(), std::not_equal_to<>()) == timeoutVec.end()) {
        timeout = timeoutVec[0];
        return result;
    } else {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t LinuxSchedulerImp::getTimesliceDuration(uint64_t &timeslice, ze_bool_t getDefault) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t i = 0;
    std::vector<uint64_t> timesliceVec = {};
    timesliceVec.resize(listOfEngines.size());
    for (const auto &engineName : listOfEngines) {
        if (getDefault) {
            result = pSysfsAccess->read(engineDir + "/" + engineName + "/" + defaultTimesliceDurationMilliSecs, timeslice);
        } else {
            result = pSysfsAccess->read(engineDir + "/" + engineName + "/" + timesliceDurationMilliSecs, timeslice);
        }
        if (result == ZE_RESULT_SUCCESS) {
            timeslice = timeslice * milliSecsToMicroSecs;
            timesliceVec[i] = timeslice;
            i++;
        } else {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            return result;
        }
    }
    // check if all engines of the same type have the same scheduling param values
    if (std::adjacent_find(timesliceVec.begin(), timesliceVec.end(), std::not_equal_to<>()) == timesliceVec.end()) {
        timeslice = timesliceVec[0];
        return result;
    } else {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t LinuxSchedulerImp::getHeartbeatInterval(uint64_t &heartbeat, ze_bool_t getDefault) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t i = 0;
    std::vector<uint64_t> heartbeatVec = {};
    heartbeatVec.resize(listOfEngines.size());
    for (const auto &engineName : listOfEngines) {
        if (getDefault) {
            result = pSysfsAccess->read(engineDir + "/" + engineName + "/" + defaultHeartbeatIntervalMilliSecs, heartbeat);
        } else {
            result = pSysfsAccess->read(engineDir + "/" + engineName + "/" + heartbeatIntervalMilliSecs, heartbeat);
        }
        if (result == ZE_RESULT_SUCCESS) {
            heartbeat = heartbeat * milliSecsToMicroSecs;
            heartbeatVec[i] = heartbeat;
            i++;
        } else {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            return result;
        }
    }
    // check if all engines of the same type have the same scheduling param values
    if (std::adjacent_find(heartbeatVec.begin(), heartbeatVec.end(), std::not_equal_to<>()) == heartbeatVec.end()) {
        heartbeat = heartbeatVec[0];
        return result;
    } else {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t LinuxSchedulerImp::setPreemptTimeout(uint64_t timeout) {
    timeout = timeout / milliSecsToMicroSecs;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    for (const auto &engineName : listOfEngines) {
        result = pSysfsAccess->write(engineDir + "/" + engineName + "/" + preemptTimeoutMilliSecs, timeout);
        if (result != ZE_RESULT_SUCCESS) {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            return result;
        }
    }
    return result;
}

ze_result_t LinuxSchedulerImp::setTimesliceDuration(uint64_t timeslice) {
    timeslice = timeslice / milliSecsToMicroSecs;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    for (const auto &engineName : listOfEngines) {
        result = pSysfsAccess->write(engineDir + "/" + engineName + "/" + timesliceDurationMilliSecs, timeslice);
        if (result != ZE_RESULT_SUCCESS) {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            return result;
        }
    }
    return result;
}

ze_result_t LinuxSchedulerImp::setHeartbeatInterval(uint64_t heartbeat) {
    heartbeat = heartbeat / milliSecsToMicroSecs;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    for (const auto &engineName : listOfEngines) {
        result = pSysfsAccess->write(engineDir + "/" + engineName + "/" + heartbeatIntervalMilliSecs, heartbeat);
        if (result != ZE_RESULT_SUCCESS) {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            return result;
        }
    }
    return result;
}

ze_bool_t LinuxSchedulerImp::canControlScheduler() {
    return 1;
}

ze_result_t LinuxSchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    *pNeedReload = false;
    return pSysfsAccess->write(enableEuDebug, 1);
}

static const std::multimap<zes_engine_type_flag_t, std::string> level0EngineTypeToSysfsEngineMap = {
    {ZES_ENGINE_TYPE_FLAG_RENDER, "rcs"},
    {ZES_ENGINE_TYPE_FLAG_DMA, "bcs"},
    {ZES_ENGINE_TYPE_FLAG_MEDIA, "vcs"},
    {ZES_ENGINE_TYPE_FLAG_OTHER, "vecs"}};

static ze_result_t getNumEngineTypeAndInstancesForDevice(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines, SysfsAccess *pSysfsAccess) {
    std::vector<std::string> localListOfAllEngines = {};
    auto result = pSysfsAccess->scanDirEntries(LinuxSchedulerImp::engineDir, localListOfAllEngines);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    for_each(localListOfAllEngines.begin(), localListOfAllEngines.end(),
             [&](std::string &mappedEngine) {
                 for (auto itr = level0EngineTypeToSysfsEngineMap.begin(); itr != level0EngineTypeToSysfsEngineMap.end(); itr++) {
                     char digits[] = "0123456789";
                     auto mappedEngineName = mappedEngine.substr(0, mappedEngine.find_first_of(digits, 0));
                     if (0 == mappedEngineName.compare(itr->second.c_str())) {
                         auto ret = mapOfEngines.find(itr->first);
                         if (ret != mapOfEngines.end()) {
                             ret->second.push_back(mappedEngine);
                         } else {
                             std::vector<std::string> engineVec = {};
                             engineVec.push_back(mappedEngine);
                             mapOfEngines.emplace(itr->first, engineVec);
                         }
                     }
                 }
             });
    return result;
}

ze_result_t OsScheduler::getNumEngineTypeAndInstances(
    std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines, OsSysman *pOsSysman, ze_device_handle_t subdeviceHandle) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    return getNumEngineTypeAndInstancesForDevice(mapOfEngines, pSysfsAccess);
}

LinuxSchedulerImp::LinuxSchedulerImp(
    OsSysman *pOsSysman, zes_engine_type_flag_t type, std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice,
    uint32_t subdeviceId) : engineType(type), onSubdevice(isSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    this->listOfEngines = listOfEngines;
}

OsScheduler *OsScheduler::create(
    OsSysman *pOsSysman, zes_engine_type_flag_t type, std::vector<std::string> &listOfEngines, ze_bool_t isSubdevice, uint32_t subdeviceId) {
    LinuxSchedulerImp *pLinuxSchedulerImp = new LinuxSchedulerImp(pOsSysman, type, listOfEngines, isSubdevice, subdeviceId);
    return static_cast<OsScheduler *>(pLinuxSchedulerImp);
}

} // namespace L0
