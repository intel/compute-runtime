/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/pmu/mock_pmu.h"

#include <cmath>

namespace L0 {
namespace Sysman {
namespace ult {

struct SysmanPmuFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockPmuInterface> pPmuInterface;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface = std::make_unique<MockPmuInterface>(pLinuxSysmanImp);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
    }
    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
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
    auto pmuInterface = std::make_unique<MockPmuInterfaceImpForSysman>(pLinuxSysmanImp);
    pmuInterface->readFunction = openReadReturnSuccess;
    uint64_t data[2];
    int validFd = 10;
    EXPECT_EQ(0, pmuInterface->pmuRead(validFd, data, sizeof(data)));
    EXPECT_EQ(mockEventVal, data[0]);
    EXPECT_EQ(mockTimeStamp, data[1]);
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPerEventOpenThenSuccessIsReturned) {
    auto pmuInterface = std::make_unique<MockPmuInterfaceImpForSysman>(pLinuxSysmanImp);
    pmuInterface->syscallFunction = syscallReturnSuccess;
    struct perf_event_attr attr = {};
    int cpu = 0;
    attr.read_format = static_cast<uint64_t>(PERF_FORMAT_TOTAL_TIME_ENABLED);
    attr.config = 11;
    EXPECT_EQ(mockPmuFd, pmuInterface->perfEventOpen(&attr, -1, cpu, -1, 0));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingThenFailureIsReturned) {
    auto pmuInterface = std::make_unique<MockPmuInterfaceImpForSysman>(pLinuxSysmanImp);
    pmuInterface->readFunction = openReadReturnFailure;
    int validFd = 10;
    uint64_t data[2];
    EXPECT_EQ(-1, pmuInterface->pmuRead(validFd, data, sizeof(data)));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPmuInterfaceOpenAndPerfEventOpenSucceedsThenVaildFdIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 18;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint64_t config = 10;
    bool isIntegratedDevice = true;
    pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_EQ(mockPmuFd, pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenReadingGroupOfEventsUsingGroupFdThenSuccessIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 18;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

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

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 18;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->perfEventOpenResult = -1;
    uint64_t config = 10;
    bool isIntegratedDevice = true;
    pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_EQ(-1, pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenCallingPmuInterfaceOpenAndPerfEventOpenFailsAndErrNoSetBySyscallIsNotInvalidArgumentThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 18;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->perfEventOpenResult = -1;
    pPmuInterface->getErrorNoResult = EBADF;
    uint64_t config = 10;
    bool isIntegratedDevice = true;
    pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    EXPECT_EQ(-1, pLinuxSysmanImp->pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED));
}

TEST_F(SysmanPmuFixture, GivenValidPmuHandleWhenAndDomainErrorOccursThenDomainErrorIsReturnedBygetErrorNoFunction) {
    auto pmuInterface = std::make_unique<MockPmuInterfaceImpForSysman>(pLinuxSysmanImp);
    log(-1.0); // Domain error injected
    EXPECT_EQ(EDOM, pmuInterface->getErrorNo());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
