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
    int64_t mockedPerfEventOpenAndSuccessReturn(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
        return mockPmuFd;
    }

    int64_t mockedPerfEventOpenAndFailureReturn(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
        return -1;
    }

    int mockedReadCountersForGroupSuccess(int fd, uint64_t *data, ssize_t sizeOfdata) {
        data[0] = mockEventCount;
        data[1] = mockTimeStamp;
        data[2] = mockEvent1Val;
        data[3] = mockEvent2Val;
        return 0;
    }

    int mockGetErrorNoSuccess() {
        return EINVAL;
    }
    int mockGetErrorNoFailure() {
        return EBADF;
    }
    MOCK_METHOD(int, pmuRead, (int fd, uint64_t *data, ssize_t sizeOfdata), (override));
    MOCK_METHOD(int64_t, perfEventOpen, (perf_event_attr * attr, pid_t pid, int cpu, int groupFd, uint64_t flags), (override));
    MOCK_METHOD(int, getErrorNo, (), (override));
};

class PmuFsAccess : public FsAccess {};

template <>
struct Mock<PmuFsAccess> : public PmuFsAccess {
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint32_t &val), (override));
    ze_result_t readValSuccess(const std::string file, uint32_t &val) {
        val = 18;
        return ZE_RESULT_SUCCESS;
    }
};

} // namespace ult
} // namespace L0