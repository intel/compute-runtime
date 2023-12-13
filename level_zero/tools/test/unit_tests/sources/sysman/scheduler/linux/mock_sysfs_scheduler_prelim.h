/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/scheduler/linux/os_scheduler_imp.h"

namespace L0 {
namespace ult {

const std::string preemptTimeoutMilliSecs("preempt_timeout_ms");
const std::string defaultPreemptTimeoutMilliSecs(".defaults/preempt_timeout_ms");
const std::string timesliceDurationMilliSecs("timeslice_duration_ms");
const std::string defaultTimesliceDurationMilliSecs(".defaults/timeslice_duration_ms");
const std::string heartbeatIntervalMilliSecs("heartbeat_interval_ms");
const std::string defaultHeartbeatIntervalMilliSecs(".defaults/heartbeat_interval_ms");
const std::string enableEuDebug("prelim_enable_eu_debug");
const std::string engineDir("engine");
const std::vector<std::string> listOfMockedEngines = {"rcs0", "bcs0", "vcs0", "vcs1", "vecs0", "ccs0"};

uint32_t tileCount = 2u;
uint32_t numberOfEnginesInSched = 34u;

struct MockSchedulerNeoDrm : public Drm {
    using Drm::getEngineInfo;
    using Drm::setupIoctlHelper;
    const int mockFd = 0;
    MockSchedulerNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    bool sysmanQueryEngineInfo() override {

        uint16_t engineClassCopy = ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy);
        uint16_t engineClassCompute = ioctlHelper->getDrmParamValue(DrmParam::engineClassCompute);
        uint16_t engineClassVideo = ioctlHelper->getDrmParamValue(DrmParam::engineClassVideo);
        uint16_t engineClassVideoEnhance = ioctlHelper->getDrmParamValue(DrmParam::engineClassVideoEnhance);
        uint16_t engineClassInvalid = ioctlHelper->getDrmParamValue(DrmParam::engineClassInvalid);

        // Fill distanceInfos vector with dummy values
        std::vector<NEO::DistanceInfo> distanceInfos = {
            {{1, 0}, {engineClassCopy, 0}, 0},
            {{1, 0}, {engineClassCopy, 1}, 1000},
            {{1, 0}, {engineClassCompute, 0}, 0},
            {{1, 0}, {engineClassCompute, 1}, 0},
            {{1, 0}, {engineClassCompute, 2}, 0},
            {{1, 0}, {engineClassCompute, 3}, 0},
            {{1, 0}, {engineClassCompute, 4}, 1000},
            {{1, 0}, {engineClassCompute, 5}, 1000},
            {{1, 0}, {engineClassCompute, 6}, 1000},
            {{1, 0}, {engineClassCompute, 7}, 1000},
            {{1, 1}, {engineClassCopy, 0}, 1000},
            {{1, 1}, {engineClassCopy, 1}, 0},
            {{1, 1}, {engineClassCompute, 0}, 1000},
            {{1, 1}, {engineClassCompute, 1}, 1000},
            {{1, 1}, {engineClassCompute, 2}, 1000},
            {{1, 1}, {engineClassCompute, 3}, 1000},
            {{1, 1}, {engineClassCompute, 4}, 0},
            {{1, 1}, {engineClassCompute, 5}, 0},
            {{1, 1}, {engineClassCompute, 6}, 0},
            {{1, 1}, {engineClassCompute, 7}, 0},
            {{1, 1}, {engineClassInvalid, 7}, 0},
        };

        std::vector<QueryItem> queryItems{distanceInfos.size()};
        for (auto i = 0u; i < distanceInfos.size(); i++) {
            queryItems[i].queryId = PRELIM_DRM_I915_QUERY_DISTANCE_INFO;
            queryItems[i].length = sizeof(NEO::PrelimI915::prelim_drm_i915_query_distance_info);
            queryItems[i].flags = 0u;
            queryItems[i].dataPtr = reinterpret_cast<uint64_t>(&distanceInfos[i]);
        }

        // Fill i915QueryEngineInfo with dummy values

        std::vector<NEO::EngineCapabilities> i915QueryEngineInfo(numberOfEnginesInSched);
        i915QueryEngineInfo[0].engine.engineClass = engineClassCopy;
        i915QueryEngineInfo[0].engine.engineInstance = 0;
        i915QueryEngineInfo[1].engine.engineClass = engineClassCopy;
        i915QueryEngineInfo[1].engine.engineInstance = 1;
        i915QueryEngineInfo[2].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[2].engine.engineInstance = 0;
        i915QueryEngineInfo[3].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[3].engine.engineInstance = 1;
        i915QueryEngineInfo[4].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[4].engine.engineInstance = 2;
        i915QueryEngineInfo[5].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[5].engine.engineInstance = 3;
        i915QueryEngineInfo[6].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[6].engine.engineInstance = 4;
        i915QueryEngineInfo[7].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[7].engine.engineInstance = 5;
        i915QueryEngineInfo[8].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[8].engine.engineInstance = 6;
        i915QueryEngineInfo[9].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[9].engine.engineInstance = 7;
        i915QueryEngineInfo[10].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[10].engine.engineInstance = 8;
        i915QueryEngineInfo[11].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[11].engine.engineInstance = 9;
        i915QueryEngineInfo[12].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[12].engine.engineInstance = 10;
        i915QueryEngineInfo[13].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[13].engine.engineInstance = 11;
        i915QueryEngineInfo[14].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[14].engine.engineInstance = 12;
        i915QueryEngineInfo[15].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[15].engine.engineInstance = 13;
        i915QueryEngineInfo[16].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[16].engine.engineInstance = 14;
        i915QueryEngineInfo[17].engine.engineClass = engineClassVideo;
        i915QueryEngineInfo[17].engine.engineInstance = 15;
        i915QueryEngineInfo[18].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[18].engine.engineInstance = 0;
        i915QueryEngineInfo[19].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[19].engine.engineInstance = 1;
        i915QueryEngineInfo[20].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[20].engine.engineInstance = 2;
        i915QueryEngineInfo[21].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[21].engine.engineInstance = 3;
        i915QueryEngineInfo[22].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[22].engine.engineInstance = 4;
        i915QueryEngineInfo[23].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[23].engine.engineInstance = 5;
        i915QueryEngineInfo[24].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[24].engine.engineInstance = 6;
        i915QueryEngineInfo[25].engine.engineClass = engineClassVideoEnhance;
        i915QueryEngineInfo[25].engine.engineInstance = 7;
        i915QueryEngineInfo[26].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[26].engine.engineInstance = 0;
        i915QueryEngineInfo[27].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[27].engine.engineInstance = 1;
        i915QueryEngineInfo[28].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[28].engine.engineInstance = 2;
        i915QueryEngineInfo[29].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[29].engine.engineInstance = 3;
        i915QueryEngineInfo[30].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[30].engine.engineInstance = 4;
        i915QueryEngineInfo[31].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[31].engine.engineInstance = 5;
        i915QueryEngineInfo[32].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[32].engine.engineInstance = 6;
        i915QueryEngineInfo[33].engine.engineClass = engineClassCompute;
        i915QueryEngineInfo[33].engine.engineInstance = 7;

        this->engineInfo.reset(new EngineInfo(this, tileCount, distanceInfos, queryItems, i915QueryEngineInfo));
        return true;
    }

    bool queryEngineInfoMockReturnFalse() {
        return false;
    }
    void resetEngineInfo() {
        engineInfo.reset();
    }
};

class SchedulerFileProperties {
    bool isAvailable = false;
    ::mode_t mode = 0;

  public:
    SchedulerFileProperties() = default;
    SchedulerFileProperties(bool isAvailable, ::mode_t mode) : isAvailable(isAvailable), mode(mode) {}

    bool getAvailability() {
        return isAvailable;
    }

    bool hasMode(::mode_t mode) {
        return mode & this->mode;
    }
};

class MockSchedulerProcfsAccess : public ProcfsAccess {
  public:
    MockSchedulerProcfsAccess() = default;

    const pid_t pid1 = 1234;
    const pid_t pid2 = 1235;
    const pid_t pid3 = 1236;
    const pid_t pid4 = 1237;

    ze_result_t listProcessesResult = ZE_RESULT_SUCCESS;
    ze_result_t listProcesses(std::vector<::pid_t> &list) override {
        if (listProcessesResult != ZE_RESULT_SUCCESS) {
            return listProcessesResult;
        }
        list.push_back(pid1);
        list.push_back(pid2);
        list.push_back(pid3);
        list.push_back(pid4);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getFileDescriptorsResult;
    ze_result_t getFileDescriptors(const ::pid_t pid, std::vector<int> &list) override {
        // push dummy fd numbers in list vector
        if (pid == pid1) {
            list.push_back(1);
            list.push_back(2);
        } else if (pid == pid2) {
            list.push_back(1);
            list.push_back(2);
        } else if (pid == pid3) {
            list.push_back(1);
            list.push_back(2);
        } else if (pid == pid4) {
            list.push_back(1);
            list.push_back(3);
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getFileName(const ::pid_t pid, const int fd, std::string &val) override {
        if (fd == 1) {
            val = "/dev/null"; // assuming after opening /dev/null fd received is 1
            return ZE_RESULT_SUCCESS;
        }
        if (fd == 2) {
            val = "/dev/dri/renderD128"; // assuming after opening /dev/dri/renderD128 fd received is 2
            return ZE_RESULT_SUCCESS;
        }
        if (fd == 3) {
            val = "/dev/dummy"; // assuming after opening /dev/dummy fd received is 3
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ::pid_t myProcessId() override {
        return pid2;
    }

    ADDMETHOD_NOBASE_VOIDRETURN(kill, (const ::pid_t pid));
};

typedef struct SchedulerConfigValues {
    uint64_t defaultVal;
    uint64_t actualVal;
} SchedulerConfigValues_t;

typedef struct SchedulerConfig {
    SchedulerConfigValues_t timeOut;
    SchedulerConfigValues_t timeSclice;
    SchedulerConfigValues_t heartBeat;
    uint64_t euDebugEnable;
} SchedulerConfig_t;

class CalledTimesUpdate {
  public:
    CalledTimesUpdate(int &calledTimes) : calledTimesVar(calledTimes) {}
    ~CalledTimesUpdate() {
        calledTimesVar++;
    }

  private:
    int &calledTimesVar;
};

struct MockSchedulerSysfsAccess : public SysfsAccess {
    using SysfsAccess::deviceNames;

    ze_result_t mockReadFileFailureError = ZE_RESULT_SUCCESS;
    ze_result_t mockGetScanDirEntryError = ZE_RESULT_SUCCESS;

    std::vector<ze_result_t> mockReadReturnValues{ZE_RESULT_SUCCESS, ZE_RESULT_SUCCESS, ZE_RESULT_SUCCESS, ZE_RESULT_ERROR_NOT_AVAILABLE};

    uint32_t mockReadCount = 0;

    bool mockReadReturnStatus = false;
    bool mockGetValueForError = false;
    bool mockGetValueForErrorWhileWrite = false;

    std::map<std::string, SchedulerConfig_t *> engineSchedMap;
    std::map<std::string, SchedulerFileProperties> engineSchedFilePropertiesMap;

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

    ze_result_t getFileProperties(const std::string file, SchedulerFileProperties &fileProps) {
        auto iterator = engineSchedFilePropertiesMap.find(file);
        if (iterator != engineSchedFilePropertiesMap.end()) {
            fileProps = static_cast<SchedulerFileProperties>(iterator->second);
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t setFileProperties(const std::string &engine, const std::string file, bool isAvailable, ::mode_t mode) {
        auto iterator = std::find(listOfMockedEngines.begin(), listOfMockedEngines.end(), engine);
        if (iterator != listOfMockedEngines.end()) {
            engineSchedFilePropertiesMap[engineDir + "/" + engine + "/" + file] = SchedulerFileProperties(isAvailable, mode);
            return ZE_RESULT_SUCCESS;
        }
        if (engine.empty()) {
            engineSchedFilePropertiesMap[file] = SchedulerFileProperties(isAvailable, mode);
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t canWrite(const std::string file) override {
        SchedulerFileProperties fileProperties;
        ze_result_t result = getFileProperties(file, fileProperties);
        if (ZE_RESULT_SUCCESS == result) {
            if (!fileProperties.getAvailability()) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            if (!fileProperties.hasMode(S_IWUSR)) {
                return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
            }
            return result;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {

        if (mockReadReturnStatus == true) {

            if (mockReadCount < mockReadReturnValues.size()) {
                ze_result_t returnValue = mockReadReturnValues[mockReadCount];
                mockReadCount++;
                return returnValue;
            }
        }

        if (mockReadFileFailureError != ZE_RESULT_SUCCESS) {
            return mockReadFileFailureError;
        }

        if (mockGetValueForError == true) {
            return getValForError(file, val);
        }

        SchedulerFileProperties fileProperties;
        ze_result_t result = getFileProperties(file, fileProperties);
        if (ZE_RESULT_SUCCESS == result) {
            if (!fileProperties.getAvailability()) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            if (!fileProperties.hasMode(S_IRUSR)) {
                return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
            }
        } else {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        //  listOfMockedEngines is as below:
        //  [0]: "rcs0"
        //  [1]: "bcs0"
        //  [2]: "vcs0"
        //  [3]: "vcs1"
        //  [4]: "vecs0"
        //  [5]: "ccs0"
        for (std::string mappedEngine : listOfMockedEngines) {
            if (file.compare(file.length() - enableEuDebug.length(),
                             enableEuDebug.length(),
                             enableEuDebug) == 0) {
                // "prelim_enable_eu_debug" sysfs node is common node across one drm client
                // This node is not engine specific. Hence it needs to be handled separately.
                // All other engine specific nodes are handled outside of this if block
                // As "prelim_enable_eu_debug" node is common across system, hence below we could find
                // using any engine node. Lets use rcs0 here.
                auto it = engineSchedMap.find("rcs0");
                val = it->second->euDebugEnable;
                return ZE_RESULT_SUCCESS;
            }
            if (file.find(mappedEngine) == std::string::npos) {
                continue;
            }
            auto it = engineSchedMap.find(mappedEngine);
            if (it == engineSchedMap.end()) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            if (file.compare(file.length() - enableEuDebug.length(),
                             enableEuDebug.length(),
                             enableEuDebug) == 0) {
                val = it->second->euDebugEnable;
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

    int writeCalled = 0;
    std::vector<ze_result_t> writeResult{ZE_RESULT_SUCCESS};
    ze_result_t write(const std::string file, const uint64_t val) override {
        CalledTimesUpdate update(writeCalled); // increment this method's call count while exiting this method
        if (writeCalled < static_cast<int>(writeResult.size())) {
            if (writeResult[writeCalled] != ZE_RESULT_SUCCESS) {
                // If we are here, it means test simply wants to validate failure values from this method
                return writeResult[writeCalled];
            }
        }

        if (mockGetValueForErrorWhileWrite == true) {
            return getValForErrorWhileWrite(file, val);
        }

        SchedulerFileProperties fileProperties;
        ze_result_t result = getFileProperties(file, fileProperties);
        if (ZE_RESULT_SUCCESS == result) {
            if (!fileProperties.getAvailability()) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            if (!fileProperties.hasMode(S_IWUSR)) {
                return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
            }
        } else {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        if (file.compare(file.length() - enableEuDebug.length(), enableEuDebug.length(), enableEuDebug) == 0) {
            // "prelim_enable_eu_debug" sysfs node is common node across one drm client
            // This node is not engine specific. Hence it needs to be handled separately.
            // All other engine specific nodes are handled outside of this if block inside for loop
            for (auto &mappedEngine : listOfMockedEngines) {
                SchedulerConfig_t *schedConfig = new SchedulerConfig_t();
                schedConfig->euDebugEnable = val;
                auto ret = engineSchedMap.emplace(mappedEngine, schedConfig);
                if (ret.second == false) {
                    auto itr = engineSchedMap.find(mappedEngine);
                    itr->second->euDebugEnable = val;
                    delete schedConfig;
                }
            }
            return ZE_RESULT_SUCCESS;
        }

        for (std::string mappedEngine : listOfMockedEngines) { // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
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

    ze_result_t scanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) override {

        if (mockGetScanDirEntryError != ZE_RESULT_SUCCESS) {
            return mockGetScanDirEntryError;
        }

        if (!isDirectoryAccessible(engineDir)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        if (!(engineDirectoryPermissions & S_IRUSR)) {
            return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        }

        listOfEntries = listOfMockedEngines;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getscanDirEntriesStatusReturnError(const std::string file, std::vector<std::string> &listOfEntries) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    void setEngineDirectoryPermission(::mode_t permission) {
        engineDirectoryPermissions = permission;
    }

    MockSchedulerSysfsAccess() = default;

  private:
    ::mode_t engineDirectoryPermissions = S_IRUSR | S_IWUSR;
    bool isDirectoryAccessible(const std::string dir) {
        if (dir.compare(engineDir) == 0) {
            return true;
        }
        return false;
    }
};

class PublicLinuxSchedulerImp : public L0::LinuxSchedulerImp {
  public:
    using LinuxSchedulerImp::isComputeUnitDebugModeEnabled;
    using LinuxSchedulerImp::pDevice;
    using LinuxSchedulerImp::pSysfsAccess;
    using LinuxSchedulerImp::setExclusiveModeImp;
    using LinuxSchedulerImp::setHeartbeatInterval;
    using LinuxSchedulerImp::setPreemptTimeout;
    using LinuxSchedulerImp::setTimesliceDuration;
    using LinuxSchedulerImp::subdeviceId;
};

} // namespace ult
} // namespace L0
