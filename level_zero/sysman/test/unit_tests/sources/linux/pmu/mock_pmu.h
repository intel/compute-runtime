/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"

using namespace NEO;

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint64_t mockEventVal = 2u;
constexpr uint64_t mockTimeStamp = 100u;
constexpr int64_t mockPmuFd = 5;
constexpr uint64_t mockEventCount = 2u;
constexpr uint64_t mockEvent1Val = 100u;
constexpr uint64_t mockEvent2Val = 150u;
class MockPmuInterfaceImpForSysman : public L0::Sysman::PmuInterfaceImp {
  public:
    using L0::Sysman::PmuInterfaceImp::getConfigAfterFormat;
    using L0::Sysman::PmuInterfaceImp::getConfigFromEventFile;
    using L0::Sysman::PmuInterfaceImp::getErrorNo;
    using L0::Sysman::PmuInterfaceImp::perfEventOpen;
    using L0::Sysman::PmuInterfaceImp::pSysmanKmdInterface;
    using L0::Sysman::PmuInterfaceImp::readFunction;
    using L0::Sysman::PmuInterfaceImp::syscallFunction;
    MockPmuInterfaceImpForSysman(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : L0::Sysman::PmuInterfaceImp(pLinuxSysmanImp) {}
};

struct MockPmuInterface : public MockPmuInterfaceImpForSysman {
    MockPmuInterface(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : MockPmuInterfaceImpForSysman(pLinuxSysmanImp) {}

    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {
        data[0] = mockEventCount;
        data[1] = mockTimeStamp;
        data[2] = mockEvent1Val;
        data[3] = mockEvent2Val;
        return 0;
    }

    ADDMETHOD_NOBASE(perfEventOpen, int64_t, mockPmuFd, (perf_event_attr * attr, pid_t pid, int cpu, int groupFd, uint64_t flags));
    ADDMETHOD_NOBASE(getErrorNo, int, EINVAL, ());
};

} // namespace ult
} // namespace Sysman
} // namespace L0