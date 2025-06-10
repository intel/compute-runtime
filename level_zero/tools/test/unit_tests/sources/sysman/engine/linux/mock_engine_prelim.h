/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/source/sysman/engine/engine_imp.h"
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"

using namespace NEO;
namespace L0 {
namespace ult {
constexpr int64_t mockPmuFd = 10;
constexpr uint64_t mockActiveTime = 987654321;
constexpr uint16_t invalidEngineClass = UINT16_MAX;
const std::string deviceDir("device");
constexpr uint32_t numberOfMockedEnginesForSingleTileDevice = 7u;
constexpr uint32_t numberOfTiles = 2u;
constexpr uint32_t numberOfMockedEnginesForMultiTileDevice = 2u;
constexpr uint32_t singleEngineInstanceCount = 1u;

struct MockMemoryManagerInEngineSysman : public MemoryManagerMock {
    MockMemoryManagerInEngineSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

struct MockEngineNeoDrmPrelim : public Drm {
    using Drm::engineInfo;
    using Drm::setupIoctlHelper;
    const int mockFd = 0;
    MockEngineNeoDrmPrelim(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    bool mockReadSysmanQueryEngineInfo = false;
    bool mockReadSysmanQueryEngineInfoMultiDevice = false;
    static bool mockQuerySingleEngineInstance;

    bool sysmanQueryEngineInfo() override {

        if (mockReadSysmanQueryEngineInfo == true) {
            return queryEngineInfoMockReturnFalse();
        }

        if (mockReadSysmanQueryEngineInfoMultiDevice == true) {
            return queryEngineInfoForMultiDeviceFixtureMockPositiveTest();
        }

        std::vector<NEO::EngineCapabilities> i915QueryEngineInfo(numberOfMockedEnginesForSingleTileDevice);
        i915QueryEngineInfo[0].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
        i915QueryEngineInfo[0].engine.engineInstance = 0;
        i915QueryEngineInfo[1].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO;
        i915QueryEngineInfo[1].engine.engineInstance = 0;
        i915QueryEngineInfo[2].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO;
        i915QueryEngineInfo[2].engine.engineInstance = 1;
        i915QueryEngineInfo[3].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY;
        i915QueryEngineInfo[3].engine.engineInstance = 0;
        i915QueryEngineInfo[4].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE;
        i915QueryEngineInfo[4].engine.engineInstance = 0;
        i915QueryEngineInfo[5].engine.engineClass = PrelimI915::prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE;
        i915QueryEngineInfo[5].engine.engineInstance = 0;
        i915QueryEngineInfo[6].engine.engineClass = invalidEngineClass;
        i915QueryEngineInfo[6].engine.engineInstance = 0;

        if (mockQuerySingleEngineInstance == true) {
            i915QueryEngineInfo.resize(singleEngineInstanceCount);
        }

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfosPerTile{i915QueryEngineInfo};

        this->engineInfo.reset(new EngineInfo(this, engineInfosPerTile));
        return true;
    }

    bool queryEngineInfoMockReturnFalse() {
        return false;
    }

    bool queryEngineInfoForMultiDeviceFixtureMockPositiveTest() {
        // Fill distanceInfos vector with dummy values
        std::vector<NEO::DistanceInfo> distanceInfos = {
            {{1, 0}, {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, 0}, 0},
            {{1, 1}, {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO, 0}, 0}};

        std::vector<QueryItem> queryItems{distanceInfos.size()};
        for (auto i = 0u; i < distanceInfos.size(); i++) {
            queryItems[i].queryId = PRELIM_DRM_I915_QUERY_DISTANCE_INFO;
            queryItems[i].length = sizeof(PrelimI915::prelim_drm_i915_query_distance_info);
            queryItems[i].flags = 0u;
            queryItems[i].dataPtr = reinterpret_cast<uint64_t>(&distanceInfos[i]);
        }

        // Fill i915QueryEngineInfo with dummy values
        std::vector<NEO::EngineCapabilities> i915QueryEngineInfo(numberOfMockedEnginesForMultiTileDevice);
        i915QueryEngineInfo[0].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
        i915QueryEngineInfo[0].engine.engineInstance = 0;
        i915QueryEngineInfo[1].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO;
        i915QueryEngineInfo[1].engine.engineInstance = 0;

        this->engineInfo.reset(new EngineInfo(this, numberOfTiles, distanceInfos, queryItems, i915QueryEngineInfo));
        return true;
    }
};

struct MockEnginePmuInterfaceImpPrelim : public PmuInterfaceImp {
    using PmuInterfaceImp::perfEventOpen;
    MockEnginePmuInterfaceImpPrelim(LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    bool mockPmuRead = false;
    bool mockPerfEventOpenRead = false;
    int32_t mockErrorNumber = -ENOSPC;
    int32_t mockPerfEventOpenFailAtCount = 1;
    uint64_t mockTimestamp = 87654321;
    static uint32_t engineConfigIndex;
    std::vector<uint64_t> mockSingleEngineConfigs = {
        __PRELIM_I915_PMU_TOTAL_ACTIVE_TICKS(0),
        PRELIM_I915_PMU_ENGINE_TOTAL_TICKS(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, 0),
        __PRELIM_I915_PMU_TOTAL_ACTIVE_TICKS(0)};

    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {

        mockPerfEventOpenFailAtCount = std::max<int32_t>(mockPerfEventOpenFailAtCount - 1, 1);
        const bool shouldCheckForError = (mockPerfEventOpenFailAtCount == 1);
        if (shouldCheckForError && mockPerfEventOpenRead == true) {
            return mockedPerfEventOpenAndFailureReturn(attr, pid, cpu, groupFd, flags);
        }

        return mockPmuFd;
    }

    int64_t mockedPerfEventOpenAndFailureReturn(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
        errno = mockErrorNumber;
        return -1;
    }

    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {

        if (fd < 0) {
            return -1;
        }

        if (mockPmuRead == true) {
            return mockedPmuReadAndFailureReturn(fd, data, sizeOfdata);
        }

        data[0] = mockActiveTime;
        data[1] = mockTimestamp;
        data[2] = mockActiveTime;
        data[3] = mockTimestamp;
        return 0;
    }

    int mockedPmuReadAndFailureReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        return -1;
    }

    int64_t pmuInterfaceOpen(uint64_t config, int group, uint32_t format) override {

        if (group > -1 && MockEngineNeoDrmPrelim::mockQuerySingleEngineInstance == true) {
            auto testConfig = mockSingleEngineConfigs[engineConfigIndex++];
            EXPECT_EQ(config, testConfig);
        }
        return PmuInterfaceImp::pmuInterfaceOpen(config, group, format);
    }
};

struct MockEngineFsAccessPrelim : public FsAccess {

    bool mockReadVal = false;

    ze_result_t read(const std::string file, uint32_t &val) override {

        if (mockReadVal == true) {
            return readValFailure(file, val);
        }

        val = 23;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readValFailure(const std::string file, uint32_t &val) {
        val = 0;
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
};

struct MockEngineSysfsAccessPrelim : public SysfsAccess {

    bool mockReadSymLinkFailure = false;
    bool mockReadSymLinkSuccess = false;
    ze_result_t mockReadStatus = ZE_RESULT_SUCCESS;
    uint32_t mockReadVal = 0;

    ze_result_t readSymLink(const std::string file, std::string &val) override {

        if (mockReadSymLinkFailure == true) {
            return getValStringSymLinkFailure(file, val);
        }

        if (mockReadSymLinkSuccess == true) {
            return getValStringSymLinkSuccess(file, val);
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValStringSymLinkSuccess(const std::string file, std::string &val) {

        if (file.compare(deviceDir) == 0) {
            val = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0";
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValStringSymLinkFailure(const std::string file, std::string &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        val = mockReadVal;
        return mockReadStatus;
    }

    MockEngineSysfsAccessPrelim() = default;
};
} // namespace ult
} // namespace L0
