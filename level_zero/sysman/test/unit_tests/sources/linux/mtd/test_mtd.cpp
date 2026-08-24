/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mtd/mock_mtd.h"

#include <map>
#include <mtd/mtd-user.h>

namespace L0 {
namespace Sysman {
namespace ult {

// Map of errno values to expected Sysman error codes as defined in LinuxSysmanImp::getResult
static const std::map<int, ze_result_t> errnoToSysmanErrorMap = {
    {EPERM, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS},
    {EACCES, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS},
    {ENOENT, ZE_RESULT_ERROR_NOT_AVAILABLE},
    {EBUSY, ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE},
    {EIO, ZE_RESULT_ERROR_UNKNOWN} // Any other errno maps to UNKNOWN
};

struct SysmanMtdFixture : public SysmanDeviceFixture {
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pMtdDevice = MemoryTechnologyDeviceInterface::create();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

  protected:
    std::unique_ptr<MemoryTechnologyDeviceInterface> pMtdDevice = nullptr;
};

TEST_F(SysmanMtdFixture, GivenValidCallWhenCreatingMtdDeviceThenSuccessIsReturned) {
    auto mtdDevice = MemoryTechnologyDeviceInterface::create();
    EXPECT_NE(mtdDevice, nullptr);
}

TEST_F(SysmanMtdFixture, GivenValidParametersWhenErasingMtdDeviceThenRequestedRangeIsErased) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<MockMtdSysCalls> mtdSysCalls(&mockMtdSysCalls, {});

    ze_result_t result = pMtdDevice->erase(mockMtdDevicePath, mockMtdOffset, mockMtdSize);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(mockMtdDevicePath, mockMtdSysCalls.openedPath);
    EXPECT_EQ(mockMtdOffset, mockMtdSysCalls.eraseStart);
    EXPECT_EQ(static_cast<uint32_t>(mockMtdSize), mockMtdSysCalls.eraseLength);
}

TEST_F(SysmanMtdFixture, GivenOpenFailsWhenErasingMtdDeviceThenErrorIsReturned) {
    static thread_local int currentErrno = 0;
    auto mockOpenWithErrno = [](const char *pathname, int flags) -> int {
        errno = currentErrno;
        return -1;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, mockOpenWithErrno);
    size_t iterationCount = 0;

    for (const auto &[errnoValue, expectedSysmanError] : errnoToSysmanErrorMap) {
        currentErrno = errnoValue;
        iterationCount++;

        ze_result_t result = pMtdDevice->erase(mockMtdDevicePath, mockMtdOffset, mockMtdSize);
        EXPECT_EQ(result, expectedSysmanError);
    }

    EXPECT_EQ(iterationCount, errnoToSysmanErrorMap.size());
}

TEST_F(SysmanMtdFixture, GivenIoctlFailsWhenErasingMtdDeviceThenErrorIsReturned) {
    static thread_local int currentErrno = 0;

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (fd == mockMtdDeviceFd && request == memEraseCmd) {
            errno = currentErrno;
            return -1;
        }
        return -1;
    });

    size_t iterationCount = 0;
    for (const auto &[errnoValue, expectedSysmanError] : errnoToSysmanErrorMap) {
        currentErrno = errnoValue;
        iterationCount++;

        ze_result_t result = pMtdDevice->erase(mockMtdDevicePath, mockMtdOffset, mockMtdSize);
        EXPECT_EQ(result, expectedSysmanError);
    }

    EXPECT_EQ(iterationCount, errnoToSysmanErrorMap.size());
}

TEST_F(SysmanMtdFixture, GivenValidParametersWhenWritingToMtdDeviceThenDataIsWrittenAtRequestedOffset) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);
    VariableBackup<MockMtdSysCalls> mtdSysCalls(&mockMtdSysCalls, {});
    VariableBackup<int> syncCalled(&NEO::SysCalls::syncCalled, 0);

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(mockMtdDevicePath, mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(mockMtdDevicePath, mockMtdSysCalls.openedPath);
    EXPECT_EQ(static_cast<off_t>(mockMtdOffset), mockMtdSysCalls.writeOffset);
    EXPECT_EQ(static_cast<const void *>(testData), mockMtdSysCalls.writeData);
    EXPECT_EQ(sizeof(testData), mockMtdSysCalls.writeCount);
    EXPECT_EQ(1, NEO::SysCalls::syncCalled);
}

TEST_F(SysmanMtdFixture, GivenWriteFailsWhenWritingToMtdDeviceThenDeviceIsNotSynced) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        errno = EIO;
        return -1;
    });
    VariableBackup<int> syncCalled(&NEO::SysCalls::syncCalled, 0);

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(mockMtdDevicePath, mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(0, NEO::SysCalls::syncCalled);
}

TEST_F(SysmanMtdFixture, GivenOpenFailsWhenWritingToMtdDeviceThenErrorIsReturned) {
    static thread_local int currentErrno = 0;
    auto mockOpenWithErrno = [](const char *pathname, int flags) -> int {
        errno = currentErrno;
        return -1;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, mockOpenWithErrno);
    size_t iterationCount = 0;

    for (const auto &[errnoValue, expectedSysmanError] : errnoToSysmanErrorMap) {
        currentErrno = errnoValue;
        iterationCount++;

        uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
        ze_result_t result = pMtdDevice->write(mockMtdDevicePath, mockMtdOffset, testData, sizeof(testData));
        EXPECT_EQ(result, expectedSysmanError);
    }

    EXPECT_EQ(iterationCount, errnoToSysmanErrorMap.size());
}

TEST_F(SysmanMtdFixture, GivenLseekFailsWhenWritingToMtdDeviceThenErrorIsReturned) {
    static thread_local int currentErrno = 0;

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, [](int fd, off_t offset, int whence) -> off_t {
        errno = currentErrno;
        return -1;
    });

    size_t iterationCount = 0;
    for (const auto &[errnoValue, expectedSysmanError] : errnoToSysmanErrorMap) {
        currentErrno = errnoValue;
        iterationCount++;

        uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
        ze_result_t result = pMtdDevice->write(mockMtdDevicePath, mockMtdOffset, testData, sizeof(testData));
        EXPECT_EQ(result, expectedSysmanError);
    }

    EXPECT_EQ(iterationCount, errnoToSysmanErrorMap.size());
}

TEST_F(SysmanMtdFixture, GivenPartialWriteWhenWritingToMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        if (fd == mockMtdDeviceFd) {
            errno = ENOENT;
            return static_cast<ssize_t>(count / 2); // Simulate partial write
        }
        return -1;
    });

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(mockMtdDevicePath, mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(SysmanMtdFixture, GivenWriteFailsWhenWritingToMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        if (fd == mockMtdDeviceFd) {
            errno = ENOENT;
            return -1;
        }
        return -1;
    });

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(mockMtdDevicePath, mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
