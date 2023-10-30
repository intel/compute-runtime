/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockPmuInterfaceImp : public L0::Sysman::PmuInterfaceImp {
  public:
    int64_t mockPmuFd = -1;
    uint64_t mockTimestamp = 0;
    uint64_t mockActiveTime = 0;
    using PmuInterfaceImp::perfEventOpen;
    using PmuInterfaceImp::pSysmanKmdInterface;
    MockPmuInterfaceImp(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    int64_t mockPerfEventFailureReturnValue = 0;
    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {
        if (mockPerfEventFailureReturnValue == -1) {
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

class MockSysmanKmdInterfaceXe : public L0::Sysman::SysmanKmdInterfaceXe {
  public:
    using L0::Sysman::SysmanKmdInterface::pFsAccess;
    using L0::Sysman::SysmanKmdInterface::pSysfsAccess;
    MockSysmanKmdInterfaceXe(const PRODUCT_FAMILY productFamily) : SysmanKmdInterfaceXe(productFamily) {}
    ~MockSysmanKmdInterfaceXe() override = default;
};

class MockFsAccessInterface : public L0::Sysman::FsAccessInterface {
  public:
    MockFsAccessInterface() = default;
    ~MockFsAccessInterface() override = default;
};

class MockSysFsAccessInterface : public L0::Sysman::SysFsAccessInterface {
  public:
    MockSysFsAccessInterface() = default;
    ~MockSysFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0