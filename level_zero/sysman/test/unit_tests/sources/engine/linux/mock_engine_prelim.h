/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"
#include "level_zero/sysman/source/api/engine/sysman_engine_imp.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

using namespace NEO;

namespace L0 {
namespace Sysman {
namespace ult {

constexpr int64_t mockPmuFd = 10;
constexpr uint64_t mockActiveTime = 987654321;
const uint32_t microSecondsToNanoSeconds = 1000u;
constexpr uint16_t invalidEngineClass = UINT16_MAX;
const std::string deviceDir("device");
constexpr uint32_t numberOfMockedEnginesForSingleTileDevice = 7u;
constexpr uint32_t numberOfTiles = 2u;
constexpr uint32_t numberOfMockedEnginesForMultiTileDevice = 2u;

class MockEngineSysmanHwDeviceIdDrm : public MockSysmanHwDeviceIdDrm {
  public:
    using L0::Sysman::ult::MockSysmanHwDeviceIdDrm::MockSysmanHwDeviceIdDrm;
    int returnOpenFileDescriptor = -1;
    int returnCloseFileDescriptor = 0;

    int openFileDescriptor() override {
        return returnOpenFileDescriptor;
    }

    int closeFileDescriptor() override {
        return returnCloseFileDescriptor;
    }
};

struct MockEngineNeoDrmPrelim : public Drm {
    using Drm::engineInfo;
    using Drm::setupIoctlHelper;
    const int mockFd = 0;
    MockEngineNeoDrmPrelim(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}
    MockEngineNeoDrmPrelim(RootDeviceEnvironment &rootDeviceEnvironment, int mockFileDescriptor) : Drm(std::make_unique<MockEngineSysmanHwDeviceIdDrm>(mockFileDescriptor, ""), rootDeviceEnvironment) {}

    bool mockReadSysmanQueryEngineInfo = false;
    bool mockReadSysmanQueryEngineInfoMultiDevice = false;

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

struct MockEnginePmuInterfaceImpPrelim : public L0::Sysman::PmuInterfaceImp {
    using PmuInterfaceImp::perfEventOpen;
    using PmuInterfaceImp::pSysmanKmdInterface;
    MockEnginePmuInterfaceImpPrelim(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    bool mockPmuRead = false;
    bool mockPerfEventOpenReadFail = false;
    int32_t mockErrorNumber = -ENOSPC;
    int32_t mockPerfEventOpenFailAtCount = 1;
    uint64_t mockTimestamp = 87654321;

    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {

        mockPerfEventOpenFailAtCount = std::max<int32_t>(mockPerfEventOpenFailAtCount - 1, 1);
        const bool shouldCheckForError = (mockPerfEventOpenFailAtCount == 1);
        if (shouldCheckForError && mockPerfEventOpenReadFail == true) {
            return mockedPerfEventOpenAndFailureReturn(attr, pid, cpu, groupFd, flags);
        }

        return mockPmuFd;
    }

    int64_t mockedPerfEventOpenAndFailureReturn(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
        errno = mockErrorNumber;
        return -1;
    }

    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {

        if (mockPmuRead == true) {
            return mockedPmuReadAndFailureReturn(fd, data, sizeOfdata);
        }

        data[2] = mockActiveTime;
        data[3] = mockTimestamp;
        return 0;
    }

    int mockedPmuReadAndFailureReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        return -1;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
