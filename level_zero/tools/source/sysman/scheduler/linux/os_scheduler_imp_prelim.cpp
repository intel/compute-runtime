/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/scheduler/linux/os_scheduler_imp.h"

namespace L0 {

const std::string LinuxSchedulerImp::preemptTimeoutMilliSecs("preempt_timeout_ms");
const std::string LinuxSchedulerImp::defaultPreemptTimeouttMilliSecs(".defaults/preempt_timeout_ms");
const std::string LinuxSchedulerImp::timesliceDurationMilliSecs("timeslice_duration_ms");
const std::string LinuxSchedulerImp::defaultTimesliceDurationMilliSecs(".defaults/timeslice_duration_ms");
const std::string LinuxSchedulerImp::heartbeatIntervalMilliSecs("heartbeat_interval_ms");
const std::string LinuxSchedulerImp::defaultHeartbeatIntervalMilliSecs(".defaults/heartbeat_interval_ms");
const std::string LinuxSchedulerImp::engineDir("engine");
const std::string LinuxSchedulerImp::enableEuDebug("prelim_enable_eu_debug");

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
                // If we are here, it means heartbeat = 0, timeout = 0, timeslice = 0.
                if (isComputeUnitDebugModeEnabled()) {
                    *pMode = ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG;
                } else {
                    *pMode = ZES_SCHED_MODE_EXCLUSIVE;
                }
            } else {
                // If we are here it means heartbeat > 0, timeout = 0, timeslice = 0.
                // And we dont know what that mode is.
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
    return result;
}

ze_result_t LinuxSchedulerImp::setExclusiveMode(ze_bool_t *pNeedReload) {
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
            // failed to disable compute unit debug mode
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
    schedProperties.canControl = canControlScheduler();
    schedProperties.engines = this->engineType;
    schedProperties.supportedModes = (1 << ZES_SCHED_MODE_TIMEOUT) | (1 << ZES_SCHED_MODE_TIMESLICE) | (1 << ZES_SCHED_MODE_EXCLUSIVE);
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSchedulerImp::getPreemptTimeout(uint64_t &timeout, ze_bool_t getDefault) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t i = 0;
    std::vector<uint64_t> timeoutVec = {};
    std::string path = "";
    timeoutVec.resize(listOfEngines.size());
    for (const auto &engineName : listOfEngines) {
        if (getDefault) {
            path = engineDir + "/" + engineName + "/" + defaultPreemptTimeouttMilliSecs;
            result = pSysfsAccess->read(path, timeout);
        } else {
            path = engineDir + "/" + engineName + "/" + preemptTimeoutMilliSecs;
            result = pSysfsAccess->read(path, timeout);
        }
        if (result == ZE_RESULT_SUCCESS) {
            timeout = timeout * milliSecsToMicroSecs;
            timeoutVec[i] = timeout;
            i++;
        } else {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read preempt timeout from %s and returning error:0x%x \n", __FUNCTION__, path.c_str(), result);
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
    std::string path = "";
    timesliceVec.resize(listOfEngines.size());
    for (const auto &engineName : listOfEngines) {
        if (getDefault) {
            path = engineDir + "/" + engineName + "/" + defaultTimesliceDurationMilliSecs;
            result = pSysfsAccess->read(path, timeslice);
        } else {
            path = engineDir + "/" + engineName + "/" + timesliceDurationMilliSecs;
            result = pSysfsAccess->read(path, timeslice);
        }
        if (result == ZE_RESULT_SUCCESS) {
            timeslice = timeslice * milliSecsToMicroSecs;
            timesliceVec[i] = timeslice;
            i++;
        } else {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read timeslice duration from %s and returning error:0x%x \n", __FUNCTION__, path.c_str(), result);
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
    std::string path = "";
    heartbeatVec.resize(listOfEngines.size());
    for (const auto &engineName : listOfEngines) {
        if (getDefault) {
            path = engineDir + "/" + engineName + "/" + defaultHeartbeatIntervalMilliSecs;
            result = pSysfsAccess->read(path, heartbeat);
        } else {
            path = engineDir + "/" + engineName + "/" + heartbeatIntervalMilliSecs;
            result = pSysfsAccess->read(path, heartbeat);
        }
        if (result == ZE_RESULT_SUCCESS) {
            heartbeat = heartbeat * milliSecsToMicroSecs;
            heartbeatVec[i] = heartbeat;
            i++;
        } else {
            if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
                result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read heartbeat interval from %s and returning error:0x%x \n", __FUNCTION__, path.c_str(), result);
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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write Preempt timeout into engineDir/%s/preemptTimeoutMilliSecs and returning error:0x%x \n", __FUNCTION__, engineName.c_str(), result);
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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write Timeslice duration into engineDir/%s/timesliceDurationMilliSecs and returning error:0x%x \n", __FUNCTION__, engineName.c_str(), result);
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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write Heartbeat interval into engineDir/%s/heartbeatIntervalMilliSecs and returning error:0x%x \n", __FUNCTION__, engineName.c_str(), result);
            return result;
        }
    }
    return result;
}

ze_bool_t LinuxSchedulerImp::canControlScheduler() {
    return 1;
}

ze_result_t LinuxSchedulerImp::updateComputeUnitDebugNode(uint64_t val) {
    // I915 will be reloaded if we toggle value of enableEuDebug
    // Hence for graceful handling close all i915 clients before toggling enableEuDebug
    auto pDevice = pLinuxSysmanImp->getDeviceHandle();
    auto devicePtr = static_cast<DeviceImp *>(pDevice);
    NEO::ExecutionEnvironment *executionEnvironment = devicePtr->getNEODevice()->getExecutionEnvironment();
    auto restorer = std::make_unique<L0::ExecutionEnvironmentRefCountRestore>(executionEnvironment);
    pLinuxSysmanImp->releaseDeviceResources();
    ze_result_t result = pLinuxSysmanImp->gpuProcessCleanup(true);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "updateComputeUnitDebugNode: gpuProcessCleanup() failed with error code: %ld\n", result);
        return result;
    }
    result = pSysfsAccess->write(enableEuDebug, val);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "updateComputeUnitDebugNode: Write to sysfs node %s failed with error code: %ld\n", enableEuDebug.c_str(), result);
    }
    pLinuxSysmanImp->initDevice();
    return result;
}

ze_result_t LinuxSchedulerImp::setComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    auto result = setExclusiveModeImp();
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "setComputeUnitDebugMode: Exclusive mode set(precontiion to set Debug mode) failed. Error code: %ld\n", result);
        return result;
    }

    *pNeedReload = false;
    uint64_t val = 1;
    auto ret = pSysfsAccess->canWrite(enableEuDebug);
    if (ret != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "setComputeUnitDebugMode: sysfs node %s not writable. Error code: %ld\n", enableEuDebug.c_str(), result);
        return ret;
    }
    return updateComputeUnitDebugNode(val);
}

bool LinuxSchedulerImp::isComputeUnitDebugModeEnabled() {
    uint64_t val = 0;
    auto result = pSysfsAccess->read(enableEuDebug, val);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "isComputeUnitDebugModeEnabled: Read from sysfs node %s failed with error code: %ld\n", enableEuDebug.c_str(), result);
        return false;
    }
    return (val == 0) ? false : true;
}

ze_result_t LinuxSchedulerImp::disableComputeUnitDebugMode(ze_bool_t *pNeedReload) {
    *pNeedReload = false;
    uint64_t val = 0;
    return updateComputeUnitDebugNode(val);
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
        UNRECOVERABLE_IF(level0EngineType == sysfsEngineMapToLevel0EngineType.end());
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
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to scan directory entries to list all engines and returning error:0x%x \n", __FUNCTION__, result);
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
    uint32_t subdeviceId = std::numeric_limits<uint32_t>::max();
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(subdeviceHandle, subdeviceId, onSubdevice, true);
    if (onSubdevice) {
        return getNumEngineTypeAndInstancesForSubDevices(mapOfEngines, pDrm, subdeviceId);
    }

    return getNumEngineTypeAndInstancesForDevice(mapOfEngines, pSysfsAccess);
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

} // namespace L0
