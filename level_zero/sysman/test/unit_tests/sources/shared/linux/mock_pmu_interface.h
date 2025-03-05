/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockPmuInterfaceImp : public L0::Sysman::PmuInterfaceImp {
  public:
    int64_t mockPmuFd = -1;
    uint64_t mockTimestamp = 0;
    uint64_t mockActiveTime = 0;
    int32_t mockErrorNumber = -ENOSPC;
    bool mockPerfEventOpenReadFail = false;
    using PmuInterfaceImp::perfEventOpen;
    using PmuInterfaceImp::pSysmanKmdInterface;
    MockPmuInterfaceImp(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {
        if (mockPerfEventOpenReadFail == true) {
            errno = mockErrorNumber;
            return -1;
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

} // namespace ult
} // namespace Sysman
} // namespace L0