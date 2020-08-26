/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/engine_info_impl.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"
#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"

#include "sysman/engine/engine_imp.h"
#include "sysman/linux/os_sysman_imp.h"

using namespace NEO;
namespace L0 {
namespace ult {
constexpr int64_t mockPmuFd = 10;
constexpr uint64_t mockTimestamp = 87654321;
constexpr uint64_t mockActiveTime = 987654321;
const uint32_t microSecondsToNanoSeconds = 1000u;
const std::string deviceDir("device");
struct MockMemoryManagerInEngineSysman : public MemoryManagerMock {
    MockMemoryManagerInEngineSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};
class EngineNeoDrm : public Drm {
  public:
    using Drm::engineInfo;
    const int mockFd = 0;
    EngineNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceId>(mockFd, ""), rootDeviceEnvironment) {}
};
template <>
struct Mock<EngineNeoDrm> : public EngineNeoDrm {
    Mock<EngineNeoDrm>(RootDeviceEnvironment &rootDeviceEnvironment) : EngineNeoDrm(rootDeviceEnvironment) {}

    bool queryEngineInfoMockPositiveTest() {
        drm_i915_engine_info i915engineInfo[5] = {};
        i915engineInfo[0].engine.engine_class = I915_ENGINE_CLASS_RENDER;
        i915engineInfo[0].engine.engine_instance = 0;
        i915engineInfo[1].engine.engine_class = I915_ENGINE_CLASS_VIDEO;
        i915engineInfo[1].engine.engine_instance = 0;
        i915engineInfo[2].engine.engine_class = I915_ENGINE_CLASS_VIDEO;
        i915engineInfo[2].engine.engine_instance = 1;
        i915engineInfo[3].engine.engine_class = I915_ENGINE_CLASS_COPY;
        i915engineInfo[3].engine.engine_instance = 0;
        i915engineInfo[4].engine.engine_class = I915_ENGINE_CLASS_VIDEO_ENHANCE;
        i915engineInfo[4].engine.engine_instance = 0;

        this->engineInfo.reset(new EngineInfoImpl(i915engineInfo, 5));
        return true;
    }

    bool queryEngineInfoMockReturnFalse() {
        return false;
    }

    MOCK_METHOD(bool, queryEngineInfo, (), (override));
};

class MockPmuInterfaceImp : public PmuInterfaceImp {
  public:
    using PmuInterfaceImp::perfEventOpen;
    MockPmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}
};
template <>
struct Mock<MockPmuInterfaceImp> : public MockPmuInterfaceImp {
    Mock<MockPmuInterfaceImp>(LinuxSysmanImp *pLinuxSysmanImp) : MockPmuInterfaceImp(pLinuxSysmanImp) {}
    int64_t mockedPerfEventOpenAndSuccessReturn(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
        return mockPmuFd;
    }
    int64_t mockedPerfEventOpenAndFailureReturn(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
        return -1;
    }
    int mockedPmuReadSingleAndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        data[0] = mockActiveTime;
        data[1] = mockTimestamp;
        return 0;
    }
    int mockedPmuReadSingleAndFailureReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        return -1;
    }

    MOCK_METHOD(int64_t, perfEventOpen, (perf_event_attr * attr, pid_t pid, int cpu, int groupFd, uint64_t flags), (override));
    MOCK_METHOD(int, pmuReadSingle, (int fd, uint64_t *data, ssize_t sizeOfdata), (override));
};

class EngineSysfsAccess : public SysfsAccess {};
class EngineFsAccess : public FsAccess {};

template <>
struct Mock<EngineFsAccess> : public EngineFsAccess {
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint32_t &val), (override));
    ze_result_t readValSuccess(const std::string file, uint32_t &val) {
        val = 23;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t readValFailure(const std::string file, uint32_t &val) {
        val = 0;
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
};

template <>
struct Mock<EngineSysfsAccess> : public EngineSysfsAccess {
    MOCK_METHOD(ze_result_t, readSymLink, (const std::string file, std::string &buf), (override));
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

    Mock<EngineSysfsAccess>() = default;
};

} // namespace ult
} // namespace L0
