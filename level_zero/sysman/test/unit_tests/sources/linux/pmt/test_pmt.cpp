/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"

#include "mock_pmt.h"

namespace L0 {
namespace Sysman {
namespace ult {

static int mockReadLinkMultiTelemetryNodesSuccess(const char *path, char *buf, size_t bufsize) {

    std::map<std::string, std::string> fileNameLinkMap = {
        {sysfsPathTelem1, realPathTelem1},
        {sysfsPathTelem2, realPathTelem2},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
}

static int mockReadLinkSingleTelemetryNodesSuccess(const char *path, char *buf, size_t bufsize) {

    std::map<std::string, std::string> fileNameLinkMap = {
        {sysfsPathTelem1, realPathTelem1},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
}

static int mockOpenSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem1OffsetFileName || strPathName == telem2OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem1GuidFileName || strPathName == telem2GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem1TelemFileName || strPathName == telem2TelemFileName) {
        returnValue = 6;
    }
    return returnValue;
}

static ssize_t mockReadOffsetFail(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    if (fd == 4) {
        errno = ENOENT;
        return -1;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadGuidFail(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint32_t val = 0;
    if (fd == 4) {
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 5) {
        errno = ENOENT;
        return -1;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadIncorrectGuid(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint32_t val = 0;
    if (fd == 4) {
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 5) {
        val = 12345;
        memcpy(buf, &val, count);
        return count;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

using ZesPmtFixture = SysmanDeviceFixture;

HWTEST2_F(ZesPmtFixture, GivenTelemNodesAreNotAvailableWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixture, GivenOffsetReadFailsWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadOffsetFail);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixture, GivenOffsetReadFailsWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadOffsetFail);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixture, GivenGuidReadFailsWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadGuidFail);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixture, GivenGuidDoesNotGiveKeyOffsetMapWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadIncorrectGuid);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixture, GivenTelemDoesNotExistWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xb15a0edc";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

TEST_F(ZesPmtFixture, GivenKeyDoesNotExistinKeyOffsetMapWhenCallingReadValueForUnsignedIntThenFalseValueIsReturned) {
    uint32_t value = 0;
    std::string mockTelemDir = sysfsPathTelem1;
    uint64_t mockOffset = 0;
    std::map<std::string, uint64_t> keyOffsetMap = {{"PACKAGE_ENERGY", 1032}, {"SOC_TEMPERATURES", 56}};
    std::string mockKey = "ABCDE";
    EXPECT_FALSE(PlatformMonitoringTech::readValue(keyOffsetMap, mockTelemDir, mockKey, mockOffset, value));
}

TEST_F(ZesPmtFixture, GivenKeyDoesNotExistinKeyOffsetMapWhenCallingReadValueForUnsignedLongThenFalseValueIsReturned) {
    uint64_t value = 0;
    std::string mockTelemDir = sysfsPathTelem1;
    uint64_t mockOffset = 0;
    std::map<std::string, uint64_t> keyOffsetMap = {{"PACKAGE_ENERGY", 1032}, {"SOC_TEMPERATURES", 56}};
    std::string mockKey = "ABCDE";
    EXPECT_FALSE(PlatformMonitoringTech::readValue(keyOffsetMap, mockTelemDir, mockKey, mockOffset, value));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
