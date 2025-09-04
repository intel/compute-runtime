/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockPmuInterfaceImp : public L0::Sysman::PmuInterfaceImp {
  public:
    using PmuInterfaceImp::getConfigAfterFormat;
    using PmuInterfaceImp::getConfigFromEventFile;
    using PmuInterfaceImp::perfEventOpen;
    using PmuInterfaceImp::pSysmanKmdInterface;
    int64_t mockPmuFd = -1;
    uint64_t mockTimestamp = 0;
    uint64_t mockActiveTime = 0;
    uint64_t mockActiveTicksConfig = 1;
    uint64_t mockTotalTicksConfig = 2;
    int32_t mockErrorNumber = -ENOSPC;
    int32_t mockPerfEventOpenFailAtCount = 1;
    int32_t mockPmuReadFailureReturnValue = 0;
    bool mockPerfEventOpenReadFail = false;
    uint32_t mockPmuConfigCallCount = 0;
    std::vector<int32_t> mockEventConfigReturnValue = {};
    std::vector<int32_t> mockFormatConfigReturnValue = {};
    std::vector<int32_t> mockVfConfigReturnValue = {};

    MockPmuInterfaceImp(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {

        mockPerfEventOpenFailAtCount = std::max<int32_t>(mockPerfEventOpenFailAtCount - 1, 1);
        const bool shouldCheckForError = (mockPerfEventOpenFailAtCount == 1);
        if (shouldCheckForError && mockPerfEventOpenReadFail == true) {
            errno = mockErrorNumber;
            return -1;
        }

        return mockPmuFd;
    }

    int32_t pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {

        if (mockPmuReadFailureReturnValue == -1) {
            return mockPmuReadFailureReturnValue;
        }

        uint64_t dataCount = sizeOfdata / sizeof(uint64_t);

        for (uint32_t i = 2; i < dataCount; i++) {
            i % 2 ? data[i] = mockTimestamp : data[i] = mockActiveTime;
        }

        return 0;
    }

    int32_t getConfigFromEventFile(std::string_view eventFile, uint64_t &config) override {
        int32_t returnValue = 0;
        if (!mockEventConfigReturnValue.empty()) {
            returnValue = mockEventConfigReturnValue.front();
            mockEventConfigReturnValue.erase(mockEventConfigReturnValue.begin());
        }
        return returnValue;
    }

    int32_t getConfigAfterFormat(std::string_view formatDir, uint64_t &config, uint64_t engineClass, uint64_t engineInstance, uint64_t gt) override {
        int32_t returnValue = 0;
        if (!mockFormatConfigReturnValue.empty()) {
            returnValue = mockFormatConfigReturnValue.front();
            mockFormatConfigReturnValue.erase(mockFormatConfigReturnValue.begin());
            return returnValue;
        }

        if (mockPmuConfigCallCount++ < 1) {
            config = mockActiveTicksConfig;
        } else {
            config = mockTotalTicksConfig;
            mockPmuConfigCallCount = 0;
        }
        return returnValue;
    }

    int32_t getPmuConfigsForVf(std::string_view sysmanDeviceDir, uint64_t fnNumber, uint64_t &activeTicksConfig, uint64_t &totalTicksConfig) override {
        int32_t returnValue = 0;
        if (!mockVfConfigReturnValue.empty()) {
            returnValue = mockVfConfigReturnValue.front();
            mockVfConfigReturnValue.erase(mockVfConfigReturnValue.begin());
        }
        return returnValue;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0