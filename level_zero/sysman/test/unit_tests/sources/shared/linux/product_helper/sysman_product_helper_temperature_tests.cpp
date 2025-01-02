/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/linux/mock_sysfs_temperature.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperTemperatureTest = SysmanDeviceFixture;

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

inline static int mockStatSuccess(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IWUSR | S_IRUSR | S_IFREG;
    return 0;
}

static int mockOpenSuccess(const char *pathname, int flags) {
    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem1OffsetFileName || strPathName == telem2OffsetFileName || strPathName == telem3OffsetFileName || strPathName == telem5OffsetFileName || strPathName == telem6OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem1GuidFileName || strPathName == telem2GuidFileName || strPathName == telem3GuidFileName || strPathName == telem5GuidFileName || strPathName == telem6GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem1TelemFileName || strPathName == telem2TelemFileName || strPathName == telem3TelemFileName || strPathName == telem5TelemFileName || strPathName == telem6TelemFileName) {
        returnValue = 6;
    }
    return returnValue;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsDG1) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGuidReadFailsWhenGettingGlobalTemperatureThenErrorIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapNotAvailableForGuidWhenGettingGlobalMaxTemperatureThenErrorIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadComputeTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0x490e";
        } else if (fd == 6) {
            if (offset == offsetComputeTemperatures) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadCoreTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0x490e";
        } else if (fd == 6) {
            if (offset == offsetComputeTemperatures) {
                for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
                    intVal |= (uint32_t)tempArrForNoSubDevices[(computeTempIndex) + i] << (i * 8);
                }
                memcpy(buf, &intVal, sizeof(intVal));
                return sizeof(intVal);
            } else if (offset == offsetCoreTemperatures) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadSocTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0x490e";
        } else if (fd == 6) {
            if (offset == offsetComputeTemperatures) {
                for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
                    intVal |= (uint32_t)tempArrForNoSubDevices[(computeTempIndex) + i] << (i * 8);
                }
                memcpy(buf, &intVal, sizeof(intVal));
                return sizeof(intVal);
            } else if (offset == offsetCoreTemperatures) {
                for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
                    intVal |= (uint32_t)tempArrForNoSubDevices[(coreTempIndex) + i] << (i * 8);
                }
                memcpy(buf, &intVal, sizeof(intVal));
                return sizeof(intVal);
            } else if (offset == offsetSocTemperatures1) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGuidReadFailsWhenGettingGpuMaxTemperatureThenErrorIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapNotAvailableForGuidWhenGettingGpuMaxTemperatureThenErrorIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadComputeTemperatureFailsWhenGettingGpuTemperatureThenFailureIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0x490e";
        } else if (fd == 6) {
            if (offset == offsetComputeTemperatures) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGlobalTemperatureThenFailureIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsPVC) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGuidReadFailsWhenGettingGlobalTemperatureThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapNotAvailableForGuidWhenGettingGlobalMaxTemperatureThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadTileMaxTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == offsetTileMaxTemperature) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGuidReadFailsWhenGettingGpuMaxTemperatureThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapNotAvailableForGuidWhenGettingGpuMaxTemperatureThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGTMaxTemperatureFailsWhenGettingGpuTemperatureThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == offsetGtMaxTemperature) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGuidReadFailsWhenGettingMemoryMaxTemperatureThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapNotAvailableForGuidWhenGettingMemoryMaxTemperatureThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadHbmMaxDeviceTemperatureFailsWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == offsetHbm0MaxDeviceTemperature) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidHandleWhenQueryingMemoryTemperatureSupportThenTrueIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    bool result = pSysmanProductHelper->isMemoryMaxTemperatureSupported();
    EXPECT_EQ(true, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGuidReadFailsWhenGettingGlobalTemperatureThenErrorIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapNotAvailableForGuidWhenGettingGlobalMaxTemperatureThenErrorIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadSocTemperatureFailsWhenGettingGlobalTemperatureThenFailureIsReturned, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint64_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0x4f9502";
        } else if (fd == 6) {
            if (offset == offsetSocTemperatures2) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGuidReadFailsWhenGettingGpuMaxTemperatureThenErrorIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapNotAvailableForGuidWhenGettingGpuMaxTemperatureThenErrorIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t intVal = 0;
        if (fd == 4) {
            memcpy(buf, &intVal, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadSocTemperatureFailsWhenGettingGpuTemperatureThenFailureIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint64_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0x4f9502";
        } else if (fd == 6) {
            if (offset == offsetSocTemperatures2) {
                errno = ENOENT;
                return -1;
            }
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingMemoryMaxTemperatureThenUnSupportedIsReturned, IsDG2) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidHandleWhenQueryingMemoryTemperatureSupportThenFalseIsReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    bool result = pSysmanProductHelper->isMemoryMaxTemperatureSupported();
    EXPECT_EQ(false, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenQueryingIsMemoryMaxTemperatureSupportedThenTrueIsReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isMemoryMaxTemperatureSupported());
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBMG) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 4) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            count = -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string invalidGuid = "0xABCDFEF";
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, invalidGuid.data(), count);
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGpuMaxTemperatureThenValidValueIsReturned, IsBMG) {
    static uint32_t gpuMaxTemperature = 10;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2f8210";
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &gpuMaxTemperature, count);
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<double>(gpuMaxTemperature), temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBMG) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 4) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            count = -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string invalidGuid = "0xABCDFEF";
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, invalidGuid.data(), count);
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingMemoryMaxTemperatureThenValidValueIsReturned, IsBMG) {
    static uint32_t memoryMaxTemperature = 10;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2f8210";
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &memoryMaxTemperature, count);
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<double>(memoryMaxTemperature), temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndGetGpuMaxTemperatureFailsThenFauilureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2f8210";
        long gpuMaxTemperatureKeyOffset = 164;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                count = -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndGetMemoryMaxTemperatureFailsThenFauilureIsReturned, IsBMG) {
    static uint32_t gpuMaxTemperature = 10;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2f8210";
        long gpuMaxTemperatureKeyOffset = 164;
        long memoryMaxTemperatureKeyOffset = 168;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &gpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                count = -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureThenValidValueIsReturned, IsBMG) {
    static uint32_t gpuMaxTemperature = 10;
    static uint32_t memoryMaxTemperature = 20;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2f8210";
        long gpuMaxTemperatureKeyOffset = 164;
        long memoryMaxTemperatureKeyOffset = 168;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &gpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto expectedValue = std::max(static_cast<double>(gpuMaxTemperature), static_cast<double>(memoryMaxTemperature));
    EXPECT_EQ(expectedValue, temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenZesGetTemperatureStateIsCalledThenValidTemperatureValueForEachSensorTypeIsReturned, IsBMG) {
    static uint32_t gpuMaxTemperature = 10;
    static uint32_t memoryMaxTemperature = 20;
    static uint32_t validTemperatureHandleCount = 3u;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2f8210";
        long gpuMaxTemperatureKeyOffset = 164;
        long memoryMaxTemperatureKeyOffset = 168;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &gpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            }
        }
        return count;
    });

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, validTemperatureHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, validTemperatureHandleCount);

    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, handles.data()));

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;

        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(gpuMaxTemperature));
        } else if (properties.type == ZES_TEMP_SENSORS_MEMORY) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(memoryMaxTemperature));
        } else if (properties.type == ZES_TEMP_SENSORS_GLOBAL) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(std::max(gpuMaxTemperature, memoryMaxTemperature)));
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
