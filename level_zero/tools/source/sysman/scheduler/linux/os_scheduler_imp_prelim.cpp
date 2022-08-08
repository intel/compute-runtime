/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include "level_zero/tools/source/sysman/scheduler/linux/os_scheduler_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const std::string LinuxSchedulerImp::preemptTimeoutMilliSecs("preempt_timeout_ms");
const std::string LinuxSchedulerImp::defaultPreemptTimeouttMilliSecs(".defaults/preempt_timeout_ms");
const std::string LinuxSchedulerImp::timesliceDurationMilliSecs("timeslice_duration_ms");
const std::string LinuxSchedulerImp::defaultTimesliceDurationMilliSecs(".defaults/timeslice_duration_ms");
const std::string LinuxSchedulerImp::heartbeatIntervalMilliSecs("heartbeat_interval_ms");
const std::string LinuxSchedulerImp::defaultHeartbeatIntervalMilliSecs(".defaults/heartbeat_interval_ms");
const std::string LinuxSchedulerImp::engineDir("engine");
constexpr uint16_t milliSecsToMicroSecs = 1000;

static const std::map<__u16, std::string> i915EngineClassToSysfsEngineMap = {
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, "rcs"},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COMPUTE), "ccs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, "bcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO, "vcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE, "vecs"}};

static const std::map<std::string, zes_engine_type_flag_t> sysfsEngineMapToLevel0EngineType = {
    {"rcs", ZES_ENGINE_TYPE_FLAG_RENDER},
    {"ccs", ZES_ENGINE_TYPE_FLAG_COMPUTE},
    {"bcs", ZES_ENGINE_TYPE_FLAG_DMA},
    {"vcs", ZES_ENGINE_TYPE_FLAG_MEDIA},
    {"vecs", ZES_ENGINE_TYPE_FLAG_OTHER}};

static const std::multimap<zes_engine_type_flag_t, std::string> level0EngineTypeToSysfsEngineMap = {
    {ZES_ENGINE_TYPE_FLAG_RENDER, "rcs"},
    {ZES_ENGINE_TYPE_FLAG_COMPUTE, "ccs"},
    {ZES_ENGINE_TYPE_FLAG_DMA, "bcs"},
    {ZES_ENGINE_TYPE_FLAG_MEDIA, "vcs"},
    {ZES_ENGINE_TYPE_FLAG_OTHER, "vecs"}};

static const std::map<std::string, __u16> sysfsEngineMapToi915EngineClass = {
    {"rcs", drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER},
    {"ccs", static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COMPUTE)},
    {"bcs", drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY},
    {"vcs", drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO},
    {"vecs", drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE}};

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

    if (engineType == ZES_ENGINE_TYPE_FLAG_COMPUTE) {
        timeout = *std::max_element(timeoutVec.begin(), timeoutVec.end());
        return result;
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

static ze_result_t getNumEngineTypeAndInstancesForSubDevices(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                             NEO::Drm *pDrm, uint32_t subdeviceId) {
    auto engineInfo = pDrm->getEngineInfo();
    if (engineInfo == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    std::vector<NEO::EngineClassInstance> listOfEngines;
    engineInfo->getListOfEnginesOnATile(subdeviceId, listOfEngines);
    for (const auto &engine : listOfEngines) {
        auto sysfEngineString = i915EngineClassToSysfsEngineMap.find(static_cast<drm_i915_gem_engine_class>(engine.engineClass));
        if (sysfEngineString == i915EngineClassToSysfsEngineMap.end()) {
            continue;
        }

        std::string sysfsEngineDirNode = sysfEngineString->second + std::to_string(engine.engineInstance);
        auto level0EngineType = sysfsEngineMapToLevel0EngineType.find(sysfEngineString->second);
        auto ret = mapOfEngines.find(level0EngineType->second);
        if (ret != mapOfEngines.end()) {
            ret->second.push_back(sysfsEngineDirNode);
        } else {
            std::vector<std::string> engineVec = {};
            engineVec.push_back(sysfsEngineDirNode);
            mapOfEngines.emplace(level0EngineType->second, engineVec);
        }
    }
    return ZE_RESULT_SUCCESS;
}

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
    auto pDrm = &pLinuxSysmanImp->getDrm();
    auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    ze_device_properties_t deviceProperties = {};
    Device::fromHandle(subdeviceHandle)->getProperties(&deviceProperties);
    if (deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) {
        return getNumEngineTypeAndInstancesForSubDevices(mapOfEngines, pDrm, deviceProperties.subdeviceId);
    }

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
