/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/linux/mock_sysfs_temperature.h"

#include <bit>
#include <limits>

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

static uint64_t packAmbientTemperatures(uint32_t rawTemperature1, uint32_t rawTemperature2) {
    return (static_cast<uint64_t>(rawTemperature2) << 32) | static_cast<uint64_t>(rawTemperature1);
}

static uint32_t toIeee754(float temperature) {
    uint32_t rawTemperature = 0;
    memcpy(&rawTemperature, &temperature, sizeof(rawTemperature));
    return rawTemperature;
}

static uint32_t toSocTemperatureFormat(float temperature) {
    return (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? toIeee754(temperature) : static_cast<uint32_t>(temperature);
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
    VariableBackup<int> mockErrno(&errno);
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
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
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
    VariableBackup<int> mockErrno(&errno);
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
    VariableBackup<int> mockErrno(&errno);
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
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
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
    VariableBackup<int> mockErrno(&errno);
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenQueryingIsMemoryMaxTemperatureSupportedThenTrueIsReturned, IsBmgOrCri) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isMemoryMaxTemperatureSupported());
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
    VariableBackup<int> mockErrno(&errno);
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGpuMaxTemperatureThenValidValueIsReturned, IsBmgOrCri) {
    static uint32_t rawGpuMaxTemperature = toSocTemperatureFormat(10.0f);
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "";
        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid = "0x5e2fa230";
        } else {
            validGuid = "0x5e2f8211";
        }
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &rawGpuMaxTemperature, count);
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(10.0, temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
    VariableBackup<int> mockErrno(&errno);
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingMemoryMaxTemperatureThenValidValueIsReturned, IsBmgOrCri) {
    static uint32_t memoryMaxTemperature = 10;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "";
        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid = "0x5e2fa230";
        } else {
            validGuid = "0x5e2f8211";
        }
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndGetGpuMaxTemperatureFailsThenFailureIsReturned, IsBmgOrCri) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "";
        long gpuMaxTemperatureKeyOffset = 0;
        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid = "0x5e2fa230";
            gpuMaxTemperatureKeyOffset = 128;
        } else {
            validGuid = "0x5e2f8211";
            gpuMaxTemperatureKeyOffset = 164;
        }

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                errno = ENOENT;
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndGetMemoryMaxTemperatureFailsThenFailureIsReturned, IsBmgOrCri) {
    static uint32_t rawGpuMaxTemperature = toSocTemperatureFormat(10.0f);
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "";
        long gpuMaxTemperatureKeyOffset = 0;
        long memoryMaxTemperatureKeyOffset = 0;
        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid = "0x5e2fa230";
            gpuMaxTemperatureKeyOffset = 128;
            memoryMaxTemperatureKeyOffset = 132;
        } else {
            validGuid = "0x5e2f8211";
            gpuMaxTemperatureKeyOffset = 164;
            memoryMaxTemperatureKeyOffset = 168;
        }

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &rawGpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                errno = ENOENT;
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureThenValidValueIsReturned, IsBmgOrCri) {
    static uint32_t rawGpuMaxTemperature = toSocTemperatureFormat(10.0f);
    static uint32_t memoryMaxTemperature = 20;
    static uint32_t vrTemperature[4] = {toIeee754(15.0f), toIeee754(25.0f), toIeee754(30.0f), toIeee754(35.0f)};
    static uint32_t vrTemperatureBmg[4] = {45, 25, 30, 35};
    // Sensor 1: 40 degree celsius, Sensor 2: 30 degree celsius
    static uint64_t ambientTemperature = packAmbientTemperatures(toIeee754(40.0f), toIeee754(30.0f));

    auto readLinkFunc = (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockReadLinkMultiTelemetryNodesSuccess : &mockReadLinkSingleTelemetryNodesSuccess;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, readLinkFunc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        static int guidReadCount = 0;
        uint64_t telemOffset = 0;
        std::string validGuid1 = "";
        std::string validGuid2 = "";
        long gpuMaxTemperatureKeyOffset = 0;
        long memoryMaxTemperatureKeyOffset = 0;
        long vrTemp0Offset = -1, vrTemp1Offset = -1, vrTemp2Offset = -1, vrTemp3Offset = -1;
        long ambTempOffset = -1;

        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid1 = "0x1e2fa030"; // VR and ambient temps
            validGuid2 = "0x5e2fa230"; // SOC and VRAM temps
            gpuMaxTemperatureKeyOffset = 128;
            memoryMaxTemperatureKeyOffset = 132;
            vrTemp0Offset = 224;
            vrTemp1Offset = 228;
            vrTemp2Offset = 232;
            vrTemp3Offset = 236;
            ambTempOffset = 176;
        } else {
            // BMG has all temperature data in one GUID: 0x5e2f8211
            validGuid1 = "0x5e2f8211"; // OOBMSM GUID for BMG with GPU, VRAM, and VR temps
            gpuMaxTemperatureKeyOffset = 164;
            memoryMaxTemperatureKeyOffset = 168;
            vrTemp0Offset = 244;
            vrTemp1Offset = 248;
            vrTemp2Offset = 252;
            vrTemp3Offset = 256;
        }

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            // For CRI: Alternate between two GUIDs since data is in different nodes
            // For BMG: Always return same GUID since all temp data is in one node
            if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
                if (guidReadCount % 2 == 0) {
                    memcpy(buf, validGuid1.data(), count);
                } else {
                    memcpy(buf, validGuid2.data(), count);
                }
                guidReadCount++;
            } else {
                memcpy(buf, validGuid1.data(), count); // Always return 0x5e2f8211 for BMG
            }
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &rawGpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTemp0Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[0] : &vrTemperatureBmg[0], count);
            } else if (offset == vrTemp1Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[1] : &vrTemperatureBmg[1], count);
            } else if (offset == vrTemp2Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[2] : &vrTemperatureBmg[2], count);
            } else if (offset == vrTemp3Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[3] : &vrTemperatureBmg[3], count);
            } else if (offset == ambTempOffset) {
                memcpy(buf, &ambientTemperature, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
        EXPECT_EQ(40.0, temperature);
    } else {
        EXPECT_EQ(45.0, temperature); // BMG uses 45 as raw VR0 value
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndgetVoltageRegulatorTemperatureFailsThenFailureIsReturned, IsBmgOrCri) {
    static uint32_t rawGpuMaxTemperature = toSocTemperatureFormat(10.0f);
    static uint32_t memoryMaxTemperature = 20;

    auto readLinkFunc = (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockReadLinkMultiTelemetryNodesSuccess : &mockReadLinkSingleTelemetryNodesSuccess;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, readLinkFunc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        static int guidReadCount = 0;
        uint64_t telemOffset = 0;
        std::string validGuid1 = "";
        std::string validGuid2 = "";
        long gpuMaxTemperatureKeyOffset = 0;
        long memoryMaxTemperatureKeyOffset = 0;
        long vrTempOffset = 0;

        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid1 = "0x1e2fa030";
            validGuid2 = "0x5e2fa230";
            gpuMaxTemperatureKeyOffset = 128;
            memoryMaxTemperatureKeyOffset = 132;
            vrTempOffset = 224;
        } else {
            // BMG has all temperature data in one GUID: 0x5e2f8211
            validGuid1 = "0x5e2f8211";
            gpuMaxTemperatureKeyOffset = 164;
            memoryMaxTemperatureKeyOffset = 168;
            vrTempOffset = 244;
        }

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
                if (guidReadCount % 2 == 0) {
                    memcpy(buf, validGuid1.data(), count);
                } else {
                    memcpy(buf, validGuid2.data(), count);
                }
                guidReadCount++;
            } else {
                memcpy(buf, validGuid1.data(), count); // Always return 0x5e2f8211 for BMG
            }
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &rawGpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTempOffset) {
                // Fail VR temperature read
                errno = ENOENT;
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndgetGpuBoardTemperatureFailsThenFailureIsReturned, IsCRI) {
    VariableBackup<int> mockErrno(&errno);
    static uint32_t rawGpuMaxTemperature = toSocTemperatureFormat(10.0f);
    static uint32_t memoryMaxTemperature = 20;
    static uint32_t vrTemperature[4] = {toIeee754(15.0f), toIeee754(25.0f), toIeee754(30.0f), toIeee754(35.0f)};

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        static int guidReadCount = 0;
        uint64_t telemOffset = 0;
        std::string validGuid1 = "";
        std::string validGuid2 = "";
        long gpuMaxTemperatureKeyOffset = 0;
        long memoryMaxTemperatureKeyOffset = 0;
        long vrTemp0Offset = 0, vrTemp1Offset = 0, vrTemp2Offset = 0, vrTemp3Offset = 0;
        long ambTempOffset = 176;

        validGuid1 = "0x1e2fa030";
        validGuid2 = "0x5e2fa230";
        gpuMaxTemperatureKeyOffset = 128;
        memoryMaxTemperatureKeyOffset = 132;
        vrTemp0Offset = 224;
        vrTemp1Offset = 228;
        vrTemp2Offset = 232;
        vrTemp3Offset = 236;

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            if (guidReadCount % 2 == 0) {
                memcpy(buf, validGuid1.data(), count);
            } else {
                memcpy(buf, validGuid2.data(), count);
            }
            guidReadCount++;
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &rawGpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTemp0Offset) {
                memcpy(buf, &vrTemperature[0], count);
            } else if (offset == vrTemp1Offset) {
                memcpy(buf, &vrTemperature[1], count);
            } else if (offset == vrTemp2Offset) {
                memcpy(buf, &vrTemperature[2], count);
            } else if (offset == vrTemp3Offset) {
                memcpy(buf, &vrTemperature[3], count);
            } else if (offset == ambTempOffset) {
                errno = ENOENT;
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenZesGetTemperatureStateIsCalledThenValidTemperatureValueForEachSensorTypeIsReturned, IsBmgOrCri) {
    static uint32_t rawGpuMaxTemperature = toSocTemperatureFormat(10.0f);
    static uint32_t memoryMaxTemperature = 20;
    static uint32_t vrTemperature[4] = {toIeee754(15.0f), toIeee754(25.0f), toIeee754(30.0f), toIeee754(35.0f)};
    static uint32_t vrTemperatureBmg[4] = {45, 25, 30, 35};
    static float compositeTemperature = 55.5f;
    static uint32_t compositeTemperatureRaw = std::bit_cast<uint32_t>(compositeTemperature);
    static uint64_t ambientTemperature = packAmbientTemperatures(toIeee754(40.0f), toIeee754(30.0f));

    auto readLinkFunc = (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockReadLinkMultiTelemetryNodesSuccess : &mockReadLinkSingleTelemetryNodesSuccess;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, readLinkFunc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        static int guidReadCount = 0;
        uint64_t telemOffset = 0;
        std::string validGuid1 = "";
        std::string validGuid2 = "";
        long gpuMaxTemperatureKeyOffset = 0;
        long memoryMaxTemperatureKeyOffset = 0;
        long compositeTempOffset = -1; // Composite sensor is not exposed on BMG
        long vrTemp0Offset = -1, vrTemp1Offset = -1, vrTemp2Offset = -1, vrTemp3Offset = -1;
        long ambTempOffset = -1;

        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid1 = "0x1e2fa030";
            validGuid2 = "0x5e2fa230";
            gpuMaxTemperatureKeyOffset = 128;
            memoryMaxTemperatureKeyOffset = 132;
            vrTemp0Offset = 224;
            vrTemp1Offset = 228;
            vrTemp2Offset = 232;
            vrTemp3Offset = 236;
            compositeTempOffset = 272;
            ambTempOffset = 176;
        } else {
            // BMG has all temperature data in one GUID: 0x5e2f8211
            validGuid1 = "0x5e2f8211";
            gpuMaxTemperatureKeyOffset = 164;
            memoryMaxTemperatureKeyOffset = 168;
            vrTemp0Offset = 244;
            vrTemp1Offset = 248;
            vrTemp2Offset = 252;
            vrTemp3Offset = 256;
        }

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            // For CRI: Alternate between two GUIDs since data is in different nodes
            // For BMG: Always return same GUID since all temp data is in one node
            if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
                if (guidReadCount % 2 == 0) {
                    memcpy(buf, validGuid1.data(), count);
                } else {
                    memcpy(buf, validGuid2.data(), count);
                }
                guidReadCount++;
            } else {
                memcpy(buf, validGuid1.data(), count); // Always return 0x5e2f8211 for BMG
            }
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &rawGpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTemp0Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[0] : &vrTemperatureBmg[0], count);
            } else if (offset == vrTemp1Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[1] : &vrTemperatureBmg[1], count);
            } else if (offset == vrTemp2Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[2] : &vrTemperatureBmg[2], count);
            } else if (offset == vrTemp3Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &vrTemperature[3] : &vrTemperatureBmg[3], count);
            } else if (offset == ambTempOffset) {
                memcpy(buf, &ambientTemperature, count);
            } else if (offset == compositeTempOffset) {
                memcpy(buf, &compositeTemperatureRaw, count);
            }
        }
        return count;
    });

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // CRI: 1 Global + 1 GPU + 1 Memory + 4 VR + 2 GPU Board + 1 Composite = 10 handles
    // BMG: 1 Global + 1 GPU + 1 Memory + 4 VR = 7 handles
    uint32_t expectedCount = (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? 10u : 7u;
    EXPECT_EQ(count, expectedCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, expectedCount);

    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, handles.data()));

    // Handles of the same sensor type are not distinguishable through the properties, hence the
    // reported values are collected and compared against the expected set of values.
    std::vector<double> vrTemperatures = {};
    std::vector<double> gpuBoardTemperatures = {};

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;

        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, 10.0);
        } else if (properties.type == ZES_TEMP_SENSORS_MEMORY) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(memoryMaxTemperature));
        } else if (properties.type == ZES_TEMP_SENSORS_GLOBAL) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            // BMG: max(gpu=10, mem=20, vr=45) = 45 (VR0=45 raw for BMG)
            if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
                EXPECT_EQ(40.0, temperature);
            } else {
                EXPECT_EQ(45.0, temperature); // BMG includes VR max (45)
            }
        } else if (properties.type == ZES_TEMP_SENSORS_VOLTAGE_REGULATOR) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            vrTemperatures.push_back(temperature);
        } else if (properties.type == ZES_TEMP_SENSORS_GPU_BOARD) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            gpuBoardTemperatures.push_back(temperature);
        } else if (properties.type == ZES_INTEL_TEMP_SENSORS_COMPOSITE_EXP) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(compositeTemperature));
        }
    }

    // Each voltage regulator sensor reports its own value
    std::vector<double> expectedVrTemperatures = {};
    if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
        expectedVrTemperatures = {15.0, 25.0, 30.0, 35.0};
    } else {
        expectedVrTemperatures = {45.0, 25.0, 30.0, 35.0};
    }
    EXPECT_EQ(vrTemperatures, expectedVrTemperatures);

    if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
        std::vector<double> expectedGpuBoardTemperatures = {40.0, 30.0};
        EXPECT_EQ(gpuBoardTemperatures, expectedGpuBoardTemperatures);
    } else {
        EXPECT_TRUE(gpuBoardTemperatures.empty());
    }
}

// CRI Temperature Tests

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenQueryingGuidToKeyOffsetMapThenValidMapIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    ASSERT_NE(nullptr, pGuidToKeyOffsetMap);
    EXPECT_GT(pGuidToKeyOffsetMap->size(), 0u);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuMaxTemperatureThenFailureIsReturned, IsCRI) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsCRI) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndGpuTemperatureIsNotAvailableWhenGettingGpuMaxTemperatureThenNotAvailableErrorIsReturned, IsCRI) {
    // SOC top die temperature reports 0xFFFFFFFF, indicating that no GPU temperature is available
    static uint32_t rawGpuMaxTemperature = 0xFFFFFFFF;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa230";
        long gpuMaxTemperatureKeyOffset = 128;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &rawGpuMaxTemperature, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGlobalMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingVRTemperatureThenFailureIsReturned, IsBmgOrCri) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuBoardTemperatureThenFailureIsReturned, IsCRI) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingVRTemperatureThenFailureIsReturned, IsBmgOrCri) {
    VariableBackup<int> mockErrno(&errno);
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
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingGpuBoardTemperatureThenFailureIsReturned, IsCRI) {
    VariableBackup<int> mockErrno(&errno);
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
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingVRTemperatureThenFailureIsReturned, IsBmgOrCri) {
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
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingGpuBoardTemperatureThenFailureIsReturned, IsCRI) {
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
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingVRTemperatureThenFailureIsReturned, IsBmgOrCri) {
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
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingGpuBoardTemperatureThenFailureIsReturned, IsCRI) {
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
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingVRTemperatureThenValidValueIsReturned, IsBmgOrCri) {
    static uint32_t mockVrTemperature[4] = {toIeee754(45.5f), toIeee754(50.0f), toIeee754(55.0f), toIeee754(60.25f)};
    static uint32_t mockVrTemperatureBmg[4] = {45, 50, 55, 60}; // BMG values: all raw values

    auto readLinkFunc = (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockReadLinkSingleTelemetryNodesSuccess : &mockReadLinkSingleTelemetryNodesSuccess;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, readLinkFunc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "";
        long vr0Offset = -1, vr1Offset = -1, vr2Offset = -1, vr3Offset = -1;

        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid = "0x1e2fa030";
            vr0Offset = 224;
            vr1Offset = 228;
            vr2Offset = 232;
            vr3Offset = 236;
        } else {
            // BMG has VR data in OOBMSM GUID: 0x5e2f8211
            validGuid = "0x5e2f8211";
            vr0Offset = 244;
            vr1Offset = 248;
            vr2Offset = 252;
            vr3Offset = 256;
        }

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockVrTemperature[0] : &mockVrTemperatureBmg[0], count);
            } else if (offset == vr1Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockVrTemperature[1] : &mockVrTemperatureBmg[1], count);
            } else if (offset == vr2Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockVrTemperature[2] : &mockVrTemperatureBmg[2], count);
            } else if (offset == vr3Offset) {
                memcpy(buf, (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? &mockVrTemperature[3] : &mockVrTemperatureBmg[3], count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    // Each voltage regulator sensor is queried individually and reports its own value
    // BMG: All VR temperatures are raw values read directly from 32-bit registers
    std::vector<double> expectedTemperatures = (defaultHwInfo->platform.eProductFamily == IGFX_CRI) ? std::vector<double>{45.5, 50.0, 55.0, 60.25} : std::vector<double>{45.0, 50.0, 55.0, 60.0};

    for (uint32_t sensorIndex = 0; sensorIndex < expectedTemperatures.size(); sensorIndex++) {
        double temperature = 0;
        ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(expectedTemperatures[sensorIndex], temperature);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGpuBoardTemperatureThenValidValueIsReturned, IsCRI) {
    // Sensor 1: 40.5 degree celsius, Sensor 2: 50.25 degree celsius
    static uint64_t mockAmbientTempContainer = packAmbientTemperatures(toIeee754(40.5f), toIeee754(50.25f));
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long ambTempOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == ambTempOffset) {
                memcpy(buf, &mockAmbientTempContainer, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    // Each ambient sensor packed in the container is queried individually
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(40.5, temperature);

    temperature = 0;
    result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(50.25, temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndOneAmbientSensorIsNotAvailableWhenGettingGpuBoardTemperatureThenNotAvailableErrorIsReturnedOnlyForThatSensor, IsCRI) {
    // Sensor 1: 42.75 degree celsius, Sensor 2: not available
    static uint64_t mockAmbientTempContainer = packAmbientTemperatures(toIeee754(42.75f), 0xFFFFFFFF);
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long ambTempOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == ambTempOffset) {
                memcpy(buf, &mockAmbientTempContainer, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    // The available sensor still reports its value, only the unavailable one returns an error
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(42.75, temperature);

    result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 1u);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndAllAmbientSensorsAreNotAvailableWhenGettingGpuBoardTemperatureThenNotAvailableErrorIsReturned, IsCRI) {
    // Both sensors report 0xFFFFFFFF, indicating that no ambient temperature is available
    static uint64_t mockAmbientTempContainer = packAmbientTemperatures(0xFFFFFFFF, 0xFFFFFFFF);
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long ambTempOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == ambTempOffset) {
                memcpy(buf, &mockAmbientTempContainer, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    for (uint32_t sensorIndex = 0; sensorIndex < 2u; sensorIndex++) {
        double temperature = 0;
        ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndSomeVRSensorsAreNotAvailableWhenGettingVRTemperatureThenNotAvailableErrorIsReturnedOnlyForThoseSensors, IsCRI) {
    // VR 0 and VR 3 report 0xFFFFFFFF, VR 1: 52.5 degree celsius, VR 2: 47 degree celsius
    static uint32_t mockVrTemperature[4] = {0xFFFFFFFF, toIeee754(52.5f), toIeee754(47.0f), 0xFFFFFFFF};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 224;
        long vr1Offset = 228;
        long vr2Offset = 232;
        long vr3Offset = 236;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                memcpy(buf, &mockVrTemperature[0], count);
            } else if (offset == vr1Offset) {
                memcpy(buf, &mockVrTemperature[1], count);
            } else if (offset == vr2Offset) {
                memcpy(buf, &mockVrTemperature[2], count);
            } else if (offset == vr3Offset) {
                memcpy(buf, &mockVrTemperature[3], count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    // Sensors reporting a valid value are unaffected by the unavailable ones
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 3u));

    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 1u));
    EXPECT_EQ(52.5, temperature);

    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 2u));
    EXPECT_EQ(47.0, temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndAllVRSensorsAreNotAvailableWhenGettingVRTemperatureThenNotAvailableErrorIsReturned, IsCRI) {
    // All four sensors report 0xFFFFFFFF, indicating that no VR temperature is available
    static uint32_t mockVrTemperature[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 224;
        long vr1Offset = 228;
        long vr2Offset = 232;
        long vr3Offset = 236;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                memcpy(buf, &mockVrTemperature[0], count);
            } else if (offset == vr1Offset) {
                memcpy(buf, &mockVrTemperature[1], count);
            } else if (offset == vr2Offset) {
                memcpy(buf, &mockVrTemperature[2], count);
            } else if (offset == vr3Offset) {
                memcpy(buf, &mockVrTemperature[3], count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    for (uint32_t sensorIndex = 0; sensorIndex < 4u; sensorIndex++) {
        double temperature = 0;
        ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndVRSensorReportsInfinityWhenGettingVRTemperatureThenInfinityIsReturned, IsCRI) {
    static uint32_t mockVrTemperature[4] = {0x00000000, toIeee754(-5.0f), 0x7F800000, toIeee754(38.5f)};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 224;
        long vr1Offset = 228;
        long vr2Offset = 232;
        long vr3Offset = 236;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                memcpy(buf, &mockVrTemperature[0], count);
            } else if (offset == vr1Offset) {
                memcpy(buf, &mockVrTemperature[1], count);
            } else if (offset == vr2Offset) {
                memcpy(buf, &mockVrTemperature[2], count);
            } else if (offset == vr3Offset) {
                memcpy(buf, &mockVrTemperature[3], count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    // VR 2 reports positive infinity, the value is reported as is for that sensor
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 2u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), temperature);

    result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(-5.0, temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingVRTemperatureFailsThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 224;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                errno = ENOENT;
                return -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingLastVRTemperatureSensorFailsThenErrorIsReturned, IsCRI) {
    static uint32_t mockVrTemperature[3] = {toIeee754(45.0f), toIeee754(50.0f), toIeee754(55.0f)};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 224;
        long vr1Offset = 228;
        long vr2Offset = 232;
        long vr3Offset = 236;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                memcpy(buf, &mockVrTemperature[0], count);
            } else if (offset == vr1Offset) {
                memcpy(buf, &mockVrTemperature[1], count);
            } else if (offset == vr2Offset) {
                memcpy(buf, &mockVrTemperature[2], count);
            } else if (offset == vr3Offset) {
                errno = ENOENT;
                return -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 3u);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    // The sensors that could be read are unaffected
    result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(45.0, temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingGpuBoardTemperatureFailsThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long ambTempOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == ambTempOffset) {
                errno = ENOENT;
                return -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenZesGetTemperatureStateIsCalledForVRThenValidTemperatureValueIsReturned, IsCRI) {
    VariableBackup<int> mockErrno(&errno);
    static uint32_t vrTemperature[4] = {toIeee754(45.0f), toIeee754(48.0f), toIeee754(43.0f), toIeee754(50.5f)};
    static uint32_t validTemperatureHandleCount = 10u;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 224;
        long vr1Offset = 228;
        long vr2Offset = 232;
        long vr3Offset = 236;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                memcpy(buf, &vrTemperature[0], count);
            } else if (offset == vr1Offset) {
                memcpy(buf, &vrTemperature[1], count);
            } else if (offset == vr2Offset) {
                memcpy(buf, &vrTemperature[2], count);
            } else if (offset == vr3Offset) {
                memcpy(buf, &vrTemperature[3], count);
            } else {
                errno = ENOENT;
                return -1;
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

    // Each voltage regulator sensor is exposed as a separate handle reporting its own value
    std::vector<double> expectedVrTemperatures = {45.0, 48.0, 43.0, 50.5};
    uint32_t vrCount = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;

        if (properties.type == ZES_TEMP_SENSORS_VOLTAGE_REGULATOR) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            ASSERT_LT(vrCount, expectedVrTemperatures.size());
            EXPECT_EQ(temperature, expectedVrTemperatures[vrCount]);
            vrCount++;
        } else if (properties.type == ZES_TEMP_SENSORS_GLOBAL || properties.type == ZES_TEMP_SENSORS_GPU) {
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetState(handle, &temperature));
        }
    }
    EXPECT_EQ(vrCount, 4u);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenZesGetTemperatureStateIsCalledForGpuBoardThenValidTemperatureValueIsReturned, IsCRI) {
    VariableBackup<int> mockErrno(&errno);
    // Sensor 1: 46 degree celsius, Sensor 2: 55 degree celsius
    static uint64_t ambientTempContainer = packAmbientTemperatures(toIeee754(46.0f), toIeee754(55.0f));
    static uint32_t validTemperatureHandleCount = 10u;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long ambTempOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == ambTempOffset) {
                memcpy(buf, &ambientTempContainer, count);
            } else {
                errno = ENOENT;
                return -1;
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

    // Each ambient sensor is exposed as a separate handle reporting its own value
    std::vector<double> expectedGpuBoardTemperatures = {46.0, 55.0};
    uint32_t gpuBoardCount = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;

        if (properties.type == ZES_TEMP_SENSORS_GPU_BOARD) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            ASSERT_LT(gpuBoardCount, expectedGpuBoardTemperatures.size());
            EXPECT_EQ(temperature, expectedGpuBoardTemperatures[gpuBoardCount]);
            gpuBoardCount++;
        } else if (properties.type == ZES_TEMP_SENSORS_GLOBAL || properties.type == ZES_TEMP_SENSORS_GPU) {
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetState(handle, &temperature));
        }
    }
    EXPECT_EQ(gpuBoardCount, 2u);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingVRTemperatureOnAnUnsupportedPlatformThenErrorIsReturned, IsAtMostBMG) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingGpuBoardTemperatureOnAnUnsupportedPlatformThenErrorIsReturned, IsAtMostBMG) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGetSupportedSensorsIsCalledThenCompositeTemperatureSensorIsIncluded, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::map<zes_temp_sensors_t, uint32_t> supportedSensorTypeMap;
    pSysmanProductHelper->getSupportedSensors(supportedSensorTypeMap);
    EXPECT_NE(supportedSensorTypeMap.find(ZES_INTEL_TEMP_SENSORS_COMPOSITE_EXP), supportedSensorTypeMap.end());
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingCompositeTemperatureThenFailureIsReturned, IsCRI) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingCompositeTemperatureThenFailureIsReturned, IsCRI) {
    VariableBackup<int> mockErrno(&errno);
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
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingCompositeTemperatureThenFailureIsReturned, IsCRI) {
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingCompositeTemperatureThenFailureIsReturned, IsCRI) {
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
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingCompositeTemperatureThenValidValueIsReturned, IsCRI) {
    static float mockCompositeTemperature = 65.5f;
    static uint32_t mockCompositeTemperatureRaw = std::bit_cast<uint32_t>(mockCompositeTemperature);
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long compositeTempOffset = 272;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == compositeTempOffset) {
                memcpy(buf, &mockCompositeTemperatureRaw, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<double>(mockCompositeTemperature), temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndCompositeTemperatureIsNotReportedByHardwareWhenGettingCompositeTemperatureThenErrorIsReturned, IsCRI) {
    static uint32_t mockCompositeTemperatureRaw = 0xFFFFFFFFu; // Sentinel indicating the value is unavailable
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long compositeTempOffset = 272;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == compositeTempOffset) {
                memcpy(buf, &mockCompositeTemperatureRaw, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    EXPECT_EQ(0, temperature);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingCompositeTemperatureFailsThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<int> mockErrno(&errno);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long compositeTempOffset = 272;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == compositeTempOffset) {
                errno = ENOENT;
                return -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingCompositeTemperatureOnAnUnsupportedPlatformThenErrorIsReturned, IsAtMostBMG) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getCompositeTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenZesGetTemperatureStateIsCalledForCompositeTemperatureThenValidTemperatureValueIsReturned, IsCRI) {
    VariableBackup<int> mockErrno(&errno);
    static float compositeTemperature = 70.25f;
    static uint32_t compositeTemperatureRaw = std::bit_cast<uint32_t>(compositeTemperature);
    static uint32_t validTemperatureHandleCount = 10u;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long compositeTempOffset = 272;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memset(buf, 0, count);
            memcpy(buf, validGuid.data(), validGuid.size());
        } else if (fd == 6) {
            if (offset == compositeTempOffset) {
                memcpy(buf, &compositeTemperatureRaw, count);
            } else {
                errno = ENOENT;
                return -1;
            }
        }
        return count;
    });

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, NULL));
    EXPECT_EQ(count, validTemperatureHandleCount);

    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, handles.data()));

    uint32_t compositeCount = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature = 0;
        if (properties.type == ZES_INTEL_TEMP_SENSORS_COMPOSITE_EXP) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(compositeTemperature));
            compositeCount++;
        } else if (properties.type == ZES_TEMP_SENSORS_GLOBAL || properties.type == ZES_TEMP_SENSORS_GPU) {
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetState(handle, &temperature));
        }
    }
    EXPECT_EQ(compositeCount, 1u);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
