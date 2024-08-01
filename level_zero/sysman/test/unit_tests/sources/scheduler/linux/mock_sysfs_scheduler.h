/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

#include "level_zero/sysman/source/api/scheduler/linux/sysman_os_scheduler_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {
namespace ult {

typedef struct SchedulerConfigValues {
    uint64_t defaultVal;
    uint64_t actualVal;
} SchedulerConfigValues_t;

typedef struct SchedulerConfig {
    SchedulerConfigValues_t timeOut;
    SchedulerConfigValues_t timeSclice;
    SchedulerConfigValues_t heartBeat;
} SchedulerConfig_t;

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

struct MockSchedulerSysfsAccess : public L0::Sysman::SysFsAccessInterface {

    std::string preemptTimeout{};
    std::string defaultpreemptTimeout{};
    std::string timesliceDuration{};
    std::string defaulttimesliceDuration{};
    std::string heartbeatInterval{};
    std::string defaultheartbeatInterval{};
    std::string engineDir{};
    std::vector<std::string> listOfMockedEngines = {"rcs0", "ccs0", "bcs0", "vcs0", "vcs1", "vecs0"};

    ze_result_t mockReadFileFailureError = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteFileStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockGetScanDirEntryError = ZE_RESULT_SUCCESS;

    std::vector<ze_result_t> mockReadReturnValues{ZE_RESULT_SUCCESS, ZE_RESULT_SUCCESS, ZE_RESULT_SUCCESS, ZE_RESULT_ERROR_NOT_AVAILABLE};

    uint32_t mockReadCount = 0;
    bool mockReadReturnStatus = false;

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

        for (std::string mappedEngine : listOfMockedEngines) {
            if (file.find(mappedEngine) == std::string::npos) {
                continue;
            }
            auto it = engineSchedMap.find(mappedEngine);
            if (it == engineSchedMap.end()) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            if (file.compare((file.length() - preemptTimeout.length()),
                             preemptTimeout.length(),
                             preemptTimeout) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    val = it->second->timeOut.defaultVal;
                } else {
                    val = it->second->timeOut.actualVal;
                }
                return ZE_RESULT_SUCCESS;
            }
            if (file.compare((file.length() - timesliceDuration.length()),
                             timesliceDuration.length(),
                             timesliceDuration) == 0) {
                if (file.find(".defaults") != std::string::npos) {
                    val = it->second->timeSclice.defaultVal;
                } else {
                    val = it->second->timeSclice.actualVal;
                }
                return ZE_RESULT_SUCCESS;
            }
            if (file.compare((file.length() - heartbeatInterval.length()),
                             heartbeatInterval.length(),
                             heartbeatInterval) == 0) {
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

    ze_result_t write(const std::string file, const uint64_t val) override {

        if (mockWriteFileStatus != ZE_RESULT_SUCCESS) {
            return mockWriteFileStatus;
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

        for (std::string mappedEngine : listOfMockedEngines) { // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
            if (file.find(mappedEngine) == std::string::npos) {
                continue;
            }
            SchedulerConfig_t *schedConfig = new SchedulerConfig_t();
            if (file.compare((file.length() - preemptTimeout.length()),
                             preemptTimeout.length(),
                             preemptTimeout) == 0) {
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
            if (file.compare((file.length() - timesliceDuration.length()),
                             timesliceDuration.length(),
                             timesliceDuration) == 0) {
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
            if (file.compare((file.length() - heartbeatInterval.length()),
                             heartbeatInterval.length(),
                             heartbeatInterval) == 0) {
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
            delete schedConfig;
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

  protected:
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

struct MockSchedulerSysfsAccessI915 : MockSchedulerSysfsAccess {

    MockSchedulerSysfsAccessI915() {
        preemptTimeout = "preempt_timeout_ms";
        defaultpreemptTimeout = ".defaults/preempt_timeout_ms";
        timesliceDuration = "timeslice_duration_ms";
        defaulttimesliceDuration = ".defaults/timeslice_duration_ms";
        heartbeatInterval = "heartbeat_interval_ms";
        defaultheartbeatInterval = ".defaults/heartbeat_interval_ms";
        engineDir = "engine";
    }
};

struct MockSchedulerSysfsAccessXe : MockSchedulerSysfsAccess {

    std::string defaultMaximumJobTimeout = ".defaults/job_timeout_max";
    SchedulerConfigValues_t defaultjobTimeout{};

    MockSchedulerSysfsAccessXe() {
        preemptTimeout = "preempt_timeout_us";
        defaultpreemptTimeout = ".defaults/preempt_timeout_us";
        timesliceDuration = "timeslice_duration_us";
        defaulttimesliceDuration = ".defaults/timeslice_duration_us";
        heartbeatInterval = "job_timeout_ms";
        defaultheartbeatInterval = ".defaults/job_timeout_ms";
        engineDir = "device/tile0/gt0/engines";
    }

    ze_result_t read(const std::string file, uint64_t &val) override {

        auto status = MockSchedulerSysfsAccess::read(file, val);
        if (status == ZE_RESULT_ERROR_NOT_AVAILABLE && mockReadReturnStatus == false) {
            if (file.compare((file.length() - defaultMaximumJobTimeout.length()),
                             defaultMaximumJobTimeout.length(),
                             defaultMaximumJobTimeout) == 0) {
                val = defaultjobTimeout.defaultVal;
                return ZE_RESULT_SUCCESS;
            }
        }
        return status;
    }

    ze_result_t write(const std::string file, const uint64_t val) override {
        auto status = MockSchedulerSysfsAccess::write(file, val);
        if (status == ZE_RESULT_ERROR_NOT_AVAILABLE && mockWriteFileStatus == false) {
            if (file.compare((file.length() - defaultMaximumJobTimeout.length()),
                             defaultMaximumJobTimeout.length(),
                             defaultMaximumJobTimeout) == 0) {
                defaultjobTimeout.defaultVal = val;
                return ZE_RESULT_SUCCESS;
            }
        }
        return status;
    }
};

class PublicLinuxSchedulerImp : public L0::Sysman::LinuxSchedulerImp {
  public:
    using LinuxSchedulerImp::pSysfsAccess;
};

struct MockSchedulerNeoDrm : public Drm {
    using Drm::getEngineInfo;
    using Drm::setupIoctlHelper;
    const int mockFd = 0;
    MockSchedulerNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    bool sysmanQueryEngineInfo() override {

        StackVec<std::vector<EngineCapabilities>, 2> engineInfosPerTile;

        for (uint32_t tileId = 0; tileId < 2; tileId++) {
            std::vector<EngineCapabilities> engineInfo;
            EngineClassInstance instance{};
            instance.engineClass = DRM_XE_ENGINE_CLASS_RENDER;
            instance.engineInstance = 0;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_RENDER;
            instance.engineInstance = 1;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
            instance.engineInstance = 0;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_COMPUTE;
            instance.engineInstance = 1;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_COPY;
            instance.engineInstance = 0;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_COPY;
            instance.engineInstance = 1;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_VIDEO_DECODE;
            instance.engineInstance = 0;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_VIDEO_DECODE;
            instance.engineInstance = 1;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE;
            instance.engineInstance = 0;
            engineInfo.push_back({instance, {}});
            instance.engineClass = DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE;
            instance.engineInstance = 1;
            engineInfo.push_back({instance, {}});
            engineInfosPerTile.push_back(engineInfo);
        }

        this->engineInfo.reset(new EngineInfo(this, engineInfosPerTile));
        return true;
    }

    bool queryEngineInfoMockReturnFalse() {
        return false;
    }
    void resetEngineInfo() {
        engineInfo.reset();
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
