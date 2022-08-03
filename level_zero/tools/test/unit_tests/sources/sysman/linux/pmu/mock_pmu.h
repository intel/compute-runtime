/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"

using namespace NEO;
namespace L0 {
namespace ult {
constexpr uint64_t mockEventVal = 2u;
constexpr uint64_t mockTimeStamp = 100u;
constexpr int64_t mockPmuFd = 5;
constexpr uint64_t mockEventCount = 2u;
constexpr uint64_t mockEvent1Val = 100u;
constexpr uint64_t mockEvent2Val = 150u;
class MockPmuInterfaceImpForSysman : public PmuInterfaceImp {
  public:
    using PmuInterfaceImp::getErrorNo;
    using PmuInterfaceImp::perfEventOpen;
    using PmuInterfaceImp::readFunction;
    using PmuInterfaceImp::syscallFunction;
    MockPmuInterfaceImpForSysman(LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}
};
template <>
struct Mock<MockPmuInterfaceImpForSysman> : public MockPmuInterfaceImpForSysman {
    Mock<MockPmuInterfaceImpForSysman>(LinuxSysmanImp *pLinuxSysmanImp) : MockPmuInterfaceImpForSysman(pLinuxSysmanImp) {}

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

class PmuFsAccess : public FsAccess {};

template <>
struct Mock<PmuFsAccess> : public PmuFsAccess {
    ze_result_t read(const std::string file, uint32_t &val) override {
        val = 18;
        return ZE_RESULT_SUCCESS;
    }
};

} // namespace ult
} // namespace L0