/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/scheduler/linux/os_scheduler_imp.h"

#include "sysman/linux/fs_access.h"

namespace L0 {
namespace ult {

const std::string preemptTimeoutMilliSecs("preempt_timeout_ms");
const std::string defaultPreemptTimeoutMilliSecs(".defaults/preempt_timeout_ms");
const std::string timesliceDurationMilliSecs("timeslice_duration_ms");
const std::string defaultTimesliceDurationMilliSecs(".defaults/timeslice_duration_ms");
const std::string heartbeatIntervalMilliSecs("heartbeat_interval_ms");
const std::string defaultHeartbeatIntervalMilliSecs(".defaults/heartbeat_interval_ms");
const std::string engineDir("engine");
const std::vector<std::string> listOfMockedEngines = {"rcs0", "bcs0", "vcs0", "vcs1", "vecs0"};

class SchedulerSysfsAccess : public SysfsAccess {};
typedef struct SchedulerConfigValues {
    uint64_t defaultVal;
    uint64_t actualVal;
} SchedulerConfigValues_t;

typedef struct SchedulerConfig {
    SchedulerConfigValues_t timeOut;
    SchedulerConfigValues_t timeSclice;
    SchedulerConfigValues_t heartBeat;
} SchedulerConfig_t;

template <>
struct Mock<SchedulerSysfsAccess> : public SysfsAccess {
    std::map<std::string, SchedulerConfig_t *> engineSchedMap;

    ze_result_t getValForError(const std::string file, uint64_t &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValForErrorWhileWrite(const std::string file, const uint64_t val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    void cleanUpMap() {
        for (std::string mappedEngine : listOfMockedEngines) {
            auto it = engineSchedMap.find(mappedEngine);
            if (it != engineSchedMap.end()) {
                delete it->second;
            }
        }
    }
    ze_result_t getVal(const std::string file, uint64_t &val) {
        for (std::string mappedEngine : listOfMockedEngines) {
            if (file.find(mappedEngine) == std::string::npos) {
                continue;
            }
            auto it = engineSchedMap.find(mappedEngine);
            if (it == engineSchedMap.end()) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            if (file.compare((file.length() - preemptTimeoutMilliSecs.length()),
                             preemptTimeoutMilliSecs.length(),
                             preemptTimeoutMilliSecs) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    val = it->second->timeOut.defaultVal;
                } else {
                    val = it->second->timeOut.actualVal;
                }
                return ZE_RESULT_SUCCESS;
            }
            if (file.compare((file.length() - timesliceDurationMilliSecs.length()),
                             timesliceDurationMilliSecs.length(),
                             timesliceDurationMilliSecs) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    val = it->second->timeSclice.defaultVal;
                } else {
                    val = it->second->timeSclice.actualVal;
                }
                return ZE_RESULT_SUCCESS;
            }
            if (file.compare((file.length() - heartbeatIntervalMilliSecs.length()),
                             heartbeatIntervalMilliSecs.length(),
                             heartbeatIntervalMilliSecs) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    val = it->second->heartBeat.defaultVal;
                } else {
                    val = it->second->heartBeat.actualVal;
                }
                return ZE_RESULT_SUCCESS;
            }
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t setVal(const std::string file, const uint64_t val) {
        for (std::string mappedEngine : listOfMockedEngines) {
            if (file.find(mappedEngine) == std::string::npos) {
                continue;
            }
            SchedulerConfig_t *schedConfig = new SchedulerConfig_t();
            if (file.compare((file.length() - preemptTimeoutMilliSecs.length()),
                             preemptTimeoutMilliSecs.length(),
                             preemptTimeoutMilliSecs) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    schedConfig->timeOut.defaultVal = val;
                } else {
                    schedConfig->timeOut.actualVal = val;
                }
                auto ret = engineSchedMap.emplace(mappedEngine, schedConfig);
                if (ret.second == false) {
                    auto itr = engineSchedMap.find(mappedEngine);
                    if (file.find(".defaults") != std::string::npos) {
                        itr->second->timeOut.defaultVal = val;
                    } else {
                        itr->second->timeOut.actualVal = val;
                    }
                    delete schedConfig;
                }
                return ZE_RESULT_SUCCESS;
            }
            if (file.compare((file.length() - timesliceDurationMilliSecs.length()),
                             timesliceDurationMilliSecs.length(),
                             timesliceDurationMilliSecs) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    schedConfig->timeSclice.defaultVal = val;
                } else {
                    schedConfig->timeSclice.actualVal = val;
                }
                auto ret = engineSchedMap.emplace(mappedEngine, schedConfig);
                if (ret.second == false) {
                    auto itr = engineSchedMap.find(mappedEngine);
                    if (file.find(".defaults") != std::string::npos) {
                        itr->second->timeSclice.defaultVal = val;
                    } else {
                        itr->second->timeSclice.actualVal = val;
                    }
                    delete schedConfig;
                }
                return ZE_RESULT_SUCCESS;
            }
            if (file.compare((file.length() - heartbeatIntervalMilliSecs.length()),
                             heartbeatIntervalMilliSecs.length(),
                             heartbeatIntervalMilliSecs) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    schedConfig->heartBeat.defaultVal = val;
                } else {
                    schedConfig->heartBeat.actualVal = val;
                }
                auto ret = engineSchedMap.emplace(mappedEngine, schedConfig);
                if (ret.second == false) {
                    auto itr = engineSchedMap.find(mappedEngine);
                    if (file.find(".defaults") != std::string::npos) {
                        itr->second->heartBeat.defaultVal = val;
                    } else {
                        itr->second->heartBeat.actualVal = val;
                    }
                    delete schedConfig;
                }
                return ZE_RESULT_SUCCESS;
            }
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t getscanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) {
        if (file.compare(engineDir) == 0) {
            listOfEntries = listOfMockedEngines;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t getscanDirEntriesStatusReturnError(const std::string file, std::vector<std::string> &listOfEntries) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    Mock<SchedulerSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(ze_result_t, write, (const std::string file, const uint64_t val), (override));
    MOCK_METHOD(ze_result_t, scanDirEntries, (const std::string file, std::vector<std::string> &listOfEntries), (override));
};

class PublicLinuxSchedulerImp : public L0::LinuxSchedulerImp {
  public:
    using LinuxSchedulerImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0
