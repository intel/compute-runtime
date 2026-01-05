/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mtd/mock_mtd.h"

#include <mtd/mtd-user.h>

namespace L0 {
namespace Sysman {
namespace ult {

// Helper functions to get default values from the map
inline const std::string &getMockMtdFilePath() {
    return mockMtdPathToFdMap.begin()->first;
}

inline int getMockMtdFd() {
    return mockMtdPathToFdMap.begin()->second;
}

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

TEST_F(SysmanMtdFixture, GivenValidParametersWhenErasingMtdDeviceThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);

    ze_result_t result = pMtdDevice->erase(getMockMtdFilePath(), mockMtdOffset, mockMtdSize);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

// Test erase method - open fails
TEST_F(SysmanMtdFixture, GivenOpenFailsWhenErasingMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });

    ze_result_t result = pMtdDevice->erase(getMockMtdFilePath(), mockMtdOffset, mockMtdSize);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanMtdFixture, GivenIoctlFailsWhenErasingMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (fd == getMockMtdFd() && request == memEraseCmd) {
            errno = EIO;
            return -1;
        }
        return -1;
    });

    ze_result_t result = pMtdDevice->erase(getMockMtdFilePath(), mockMtdOffset, mockMtdSize);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanMtdFixture, GivenValidParametersWhenWritingToMtdDeviceThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekWriteSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(getMockMtdFilePath(), mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(SysmanMtdFixture, GivenOpenFailsWhenWritingToMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(getMockMtdFilePath(), mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanMtdFixture, GivenLseekFailsWhenWritingToMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, [](int fd, off_t offset, int whence) -> off_t {
        return -1; // Always fail
    });

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(getMockMtdFilePath(), mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanMtdFixture, GivenPartialWriteWhenWritingToMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekWriteSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        if (fd == getMockMtdFd()) {
            return static_cast<ssize_t>(count / 2); // Simulate partial write
        }
        return -1;
    });

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(getMockMtdFilePath(), mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanMtdFixture, GivenWriteFailsWhenWritingToMtdDeviceThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekWriteSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        if (fd == getMockMtdFd()) {
            errno = EIO;
            return -1;
        }
        return -1;
    });

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    ze_result_t result = pMtdDevice->write(getMockMtdFilePath(), mockMtdOffset, testData, sizeof(testData));
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanMtdFixture, GivenValidParametersWhenGettingDeviceInfoThenSuccessIsReturned) {
    SysCalls::lseekCalledCount = 0;
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);

    std::map<std::string, std::map<uint32_t, uint32_t>> regionInfoMap;
    ze_result_t result = pMtdDevice->getDeviceInfo(getMockMtdFilePath(), regionInfoMap);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    // Verify that region information was populated for all regions
    EXPECT_EQ(regionInfoMap.size(), static_cast<uint64_t>(mockMtdRegionCount));

    // Check if DESCRIPTOR region is present and has valid data
    auto descriptorIt = regionInfoMap.find("DESCRIPTOR");
    if (descriptorIt != regionInfoMap.end()) {
        auto &descriptorInfo = descriptorIt->second;
        auto beginIt = descriptorInfo.find(0);
        auto endIt = descriptorInfo.find(1);
        EXPECT_NE(beginIt, descriptorInfo.end());
        EXPECT_NE(endIt, descriptorInfo.end());
        EXPECT_LT(beginIt->second, endIt->second); // Verify begin < end
    }
    EXPECT_EQ(SysCalls::lseekCalledCount, static_cast<int>(mockMtdRegionCount));
}

TEST_F(SysmanMtdFixture, GivenOpenFailsWhenGettingDeviceInfoThenErrorIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });

    std::map<std::string, std::map<uint32_t, uint32_t>> regionInfoMap;
    ze_result_t result = pMtdDevice->getDeviceInfo(getMockMtdFilePath(), regionInfoMap);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

TEST_F(SysmanMtdFixture, GivenLseekFailsWhenGettingDeviceInfoThenRegionsAreSkipped) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, [](int fd, off_t offset, int whence) -> off_t {
        return -1; // Always fail
    });

    std::map<std::string, std::map<uint32_t, uint32_t>> regionInfoMap;
    ze_result_t result = pMtdDevice->getDeviceInfo(getMockMtdFilePath(), regionInfoMap);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);  // Should still succeed, but regions will be empty
    EXPECT_EQ(regionInfoMap.size(), 0ull); // No regions should be populated
}

TEST_F(SysmanMtdFixture, GivenReadFailsWhenGettingDeviceInfoThenRegionsAreSkipped) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        if (fd == getMockMtdFd()) {
            errno = EIO;
            return -1;
        }
        return -1;
    });

    std::map<std::string, std::map<uint32_t, uint32_t>> regionInfoMap;
    ze_result_t result = pMtdDevice->getDeviceInfo(getMockMtdFilePath(), regionInfoMap);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);  // Should still succeed, but regions will be empty
    EXPECT_EQ(regionInfoMap.size(), 0ull); // No regions should be populated
}

TEST_F(SysmanMtdFixture, GivenMultipleInvalidRegionRangesWhenGettingDeviceInfoThenAllRegionsAreSkipped) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        if (fd == getMockMtdFd()) {
            if (count == sizeof(MtdSysman::SpiDescRegionBar)) {
                MtdSysman::SpiDescRegionBar *regionBar = reinterpret_cast<MtdSysman::SpiDescRegionBar *>(buf);
                // Always return invalid range where base >= limit
                // This ensures regionBegin >= regionEnd in all cases
                regionBar->base = 0x200;  // base = 0x200, so regionBegin = 0x200000
                regionBar->limit = 0x100; // limit = 0x100, so regionEnd = 0x100FFF. Since 0x200000 > 0x100FFF, the condition regionBegin >= regionEnd is true
                regionBar->reserved0 = 0;
                regionBar->reserved1 = 0;
                return count;
            }
        }
        return -1;
    });

    std::map<std::string, std::map<uint32_t, uint32_t>> regionInfoMap;
    ze_result_t result = pMtdDevice->getDeviceInfo(getMockMtdFilePath(), regionInfoMap);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);  // Should still succeed, but regions will be empty
    EXPECT_EQ(regionInfoMap.size(), 0ull); // No regions should be populated due to all invalid ranges
}

TEST_F(SysmanMtdFixture, GivenIncompleteReadWhenGettingDeviceInfoThenRegionsAreSkipped) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        if (fd == getMockMtdFd()) {
            // Return fewer bytes than expected to trigger the bytesRead != sizeof(value) condition
            return count - 1; // Return one less byte than expected
        }
        return -1;
    });

    std::map<std::string, std::map<uint32_t, uint32_t>> regionInfoMap;
    ze_result_t result = pMtdDevice->getDeviceInfo(getMockMtdFilePath(), regionInfoMap);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);  // Should still succeed, but regions will be empty
    EXPECT_EQ(regionInfoMap.size(), 0ull); // No regions should be populated due to incomplete reads
}

} // namespace ult
} // namespace Sysman
} // namespace L0