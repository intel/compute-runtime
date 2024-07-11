/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/source/sysman/engine/engine_imp.h"
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"

using namespace NEO;
namespace L0 {
namespace ult {
constexpr int64_t mockPmuFd = 10;
constexpr uint64_t mockTimestamp = 87654321;
constexpr uint64_t mockActiveTime = 987654321;
const uint32_t microSecondsToNanoSeconds = 1000u;
constexpr uint16_t invalidEngineClass = UINT16_MAX;
const std::string deviceDir("device");
struct MockMemoryManagerInEngineSysman : public MemoryManagerMock {
    MockMemoryManagerInEngineSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

struct MockEngineNeoDrm : public Drm {
    using Drm::getEngineInfo;
    using Drm::setupIoctlHelper;
    const int mockFd = 0;
    MockEngineNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    bool mockSysmanQueryEngineInfoReturnFalse = true;
    bool sysmanQueryEngineInfo() override {
        if (mockSysmanQueryEngineInfoReturnFalse != true) {
            return mockSysmanQueryEngineInfoReturnFalse;
        }

        std::vector<NEO::EngineCapabilities> i915engineInfo(6);
        i915engineInfo[0].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
        i915engineInfo[0].engine.engineInstance = 0;
        i915engineInfo[1].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
        i915engineInfo[1].engine.engineInstance = 1;
        i915engineInfo[2].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO;
        i915engineInfo[2].engine.engineInstance = 1;
        i915engineInfo[3].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY;
        i915engineInfo[3].engine.engineInstance = 0;
        i915engineInfo[4].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE;
        i915engineInfo[4].engine.engineInstance = 0;
        i915engineInfo[5].engine.engineClass = invalidEngineClass;
        i915engineInfo[5].engine.engineInstance = 0;

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfosPerTile{i915engineInfo};

        this->engineInfo.reset(new EngineInfo(this, engineInfosPerTile));
        return true;
    }
};

struct MockEnginePmuInterfaceImp : public PmuInterfaceImp {
    using PmuInterfaceImp::perfEventOpen;
    MockEnginePmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    int64_t mockPerfEventFailureReturnValue = 0;
    int32_t mockErrorNumber = -ENOSPC;
    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {
        if (mockPerfEventFailureReturnValue == -1) {
            errno = mockErrorNumber;
            return mockPerfEventFailureReturnValue;
        }

        return mockPmuFd;
    }

    int mockPmuReadFailureReturnValue = 0;
    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {
        if (mockPmuReadFailureReturnValue == -1) {
            return mockPmuReadFailureReturnValue;
        }

        data[0] = mockActiveTime;
        data[1] = mockTimestamp;
        return 0;
    }
};

struct MockEngineFsAccess : public FsAccess {
    uint32_t mockReadVal = 23;
    ze_result_t mockReadErrorVal = ZE_RESULT_SUCCESS;
    ze_result_t readResult = ZE_RESULT_SUCCESS;
    ze_result_t read(const std::string file, uint32_t &val) override {
        val = mockReadVal;
        if (mockReadErrorVal != ZE_RESULT_SUCCESS) {
            readResult = mockReadErrorVal;
        }

        return readResult;
    }
};

struct MockEngineSysfsAccess : public SysfsAccess {
    ze_result_t mockReadSymLinkError = ZE_RESULT_SUCCESS;
    ze_result_t readSymLinkResult = ZE_RESULT_SUCCESS;
    uint32_t readSymLinkCalled = 0u;
    ze_result_t readSymLink(const std::string file, std::string &val) override {
        readSymLinkCalled++;
        if ((mockReadSymLinkError != ZE_RESULT_SUCCESS) && (readSymLinkCalled == 1)) {
            return mockReadSymLinkError;
        }

        if (file.compare(deviceDir) == 0) {
            val = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0";
        }
        return readSymLinkResult;
    }

    MockEngineSysfsAccess() = default;
};

using DrmMockEngineInfoFailing = DrmMock;

} // namespace ult
} // namespace L0
