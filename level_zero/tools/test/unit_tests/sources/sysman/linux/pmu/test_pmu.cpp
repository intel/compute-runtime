/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/pmu/mock_pmu.h"

#include <cmath>

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {
struct SysmanPmuFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockPmuInterface> pPmuInterface;
    PmuInterface *pOriginalPmuInterface = nullptr;
    std::unique_ptr<MockPmuFsAccess> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockPmuFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface = std::make_unique<MockPmuInterface>(pLinuxSysmanImp);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        SysmanDeviceFixture::TearDown();
    }
};

inline static ssize_t openReadReturnSuccess(int fd, void *data, size_t sizeOfdata) {
    uint64_t dataVal[2] = {mockEventVal, mockTimeStamp};
    memcpy_s(data, sizeOfdata, dataVal, sizeOfdata);
    return sizeOfdata;
}

inline static ssize_t openReadReturnFailure(int fd, void *data, size_t sizeOfdata) {
    return -1;
}

inline static long int syscallReturnSuccess(long int sysNo, ...) noexcept {
    return mockPmuFd;
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPmuReadThenSuccessIsReturned) {
    MockPmuInterfaceImpForSysman *pmuInterface = new MockPmuInterfaceImpForSysman(pLinuxSysmanImp);
    pmuInterface->readFunction = openReadReturnSuccess;
    uint64_t data[2];
    int validFd = 10;
    EXPECT_EQ(0, pmuInterface->pmuRead(validFd, data, sizeof(data)));
    EXPECT_EQ(mockEventVal, data[0]);
    EXPECT_EQ(mockTimeStamp, data[1]);
    delete pmuInterface;
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPerEventOpenThenSuccessIsReturned) {
    MockPmuInterfaceImpForSysman *pmuInterface = new MockPmuInterfaceImpForSysman(pLinuxSysmanImp);
    pmuInterface->syscallFunction = syscallReturnSuccess;
    struct perf_event_attr attr = {};
    int cpu = 0;
    attr.read_format = static_cast<uint64_t>(PERF_FORMAT_TOTAL_TIME_ENABLED);
    attr.config = 11;
    EXPECT_EQ(mockPmuFd, pmuInterface->perfEventOpen(&attr, -1, cpu, -1, 0));
    delete pmuInterface;
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingThenFailureIsReturned) {
    MockPmuInterfaceImpForSysman *pmuInterface = new MockPmuInterfaceImpForSysman(pLinuxSysmanImp);
    pmuInterface->readFunction = openReadReturnFailure;
    int validFd = 10;
    uint64_t data[2];
    EXPECT_EQ(-1, pmuInterface->pmuRead(validFd, data, sizeof(data)));
    delete pmuInterface;
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPmuInterfaceOpenAndPerfEventOpenSucceedsThenVaildFdIsReturned) {
    uint64_t config = 10;
    EXPECT_EQ(mockPmuFd, pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenReadingGroupOfEventsUsingGroupFdThenSuccessIsReturned) {
    uint64_t configForEvent1 = 10;
    int64_t groupFd = pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(configForEvent1, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP); // To get group leader
    uint64_t configForEvent2 = 15;
    pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(configForEvent2, static_cast<int>(groupFd), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
    uint64_t data[4];
    EXPECT_EQ(0, pLinuxSysmanImp->pPmuInterface->pmuRead(static_cast<int>(groupFd), data, sizeof(data)));
    EXPECT_EQ(mockEventCount, data[0]);
    EXPECT_EQ(mockTimeStamp, data[1]);
    EXPECT_EQ(mockEvent1Val, data[2]);
    EXPECT_EQ(mockEvent2Val, data[3]);
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPmuInterfaceOpenAndPerfEventOpenFailsThenFailureIsReturned) {
    pPmuInterface->perfEventOpenResult = -1;
    uint64_t config = 10;
    EXPECT_EQ(-1, pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPmuInterfaceOpenAndPerfEventOpenFailsAndErrNoSetBySyscallIsNotInvalidArgumentThenFailureIsReturned) {
    pPmuInterface->perfEventOpenResult = -1;
    pPmuInterface->getErrorNoResult = EBADF;
    uint64_t config = 10;
    EXPECT_EQ(-1, pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenAndDomainErrorOccursThenDomainErrorIsReturnedBygetErrorNoFunction) {
    MockPmuInterfaceImpForSysman *pmuInterface = new MockPmuInterfaceImpForSysman(pLinuxSysmanImp);
    log(-1.0); // Domain error injected
    EXPECT_EQ(EDOM, pmuInterface->getErrorNo());
    delete pmuInterface;
}

} // namespace ult
} // namespace L0
