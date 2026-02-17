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

namespace L0 {
namespace Sysman {
namespace ult {

using IsBmgOrCri = IsAnyProducts<IGFX_BMG, IGFX_CRI>;
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
    static uint32_t gpuMaxTemperature = 10;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "";
        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid = "0x5e2fa230";
        } else {
            validGuid = "0x5e2f8210";
        }
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingMemoryMaxTemperatureThenFailureIsReturned, IsBmgOrCri) {
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
            validGuid = "0x5e2f8210";
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "";
        long gpuMaxTemperatureKeyOffset = 0;
        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid = "0x5e2fa230";
            gpuMaxTemperatureKeyOffset = 128;
        } else {
            validGuid = "0x5e2f8210";
            gpuMaxTemperatureKeyOffset = 164;
        }

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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndGetMemoryMaxTemperatureFailsThenFailureIsReturned, IsBmgOrCri) {
    static uint32_t gpuMaxTemperature = 10;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
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
            validGuid = "0x5e2f8210";
            gpuMaxTemperatureKeyOffset = 164;
            memoryMaxTemperatureKeyOffset = 168;
        }

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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureThenValidValueIsReturned, IsBmgOrCri) {
    static uint32_t gpuMaxTemperature = 10;
    static uint32_t memoryMaxTemperature = 20;
    static uint32_t vrTemperature0 = 150; // 15.0 degC in deci-degree
    static uint32_t vrTemperature1 = 25;
    static uint32_t vrTemperature2 = 30;
    static uint32_t vrTemperature3 = 35;
    static uint32_t gpuBoardTemperature = 0x001E0028; // Index 0: 0x28 (40 degC), Index 1: 0x1E (30 degC)

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
        long vrTemp0Offset = 0, vrTemp1Offset = 0, vrTemp2Offset = 0, vrTemp3Offset = 0;
        long gpuBoardTempOffset = 0;

        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid1 = "0x1e2fa030"; // VR and GPU board temps
            validGuid2 = "0x5e2fa230"; // SOC and VRAM temps
            gpuMaxTemperatureKeyOffset = 128;
            memoryMaxTemperatureKeyOffset = 132;
            vrTemp0Offset = 200;
            vrTemp1Offset = 204;
            vrTemp2Offset = 208;
            vrTemp3Offset = 212;
            gpuBoardTempOffset = 176;
        } else {
            // BMG has all temperature data in one GUID: 0x5e2f8210
            validGuid1 = "0x5e2f8210"; // OOBMSM GUID for BMG with GPU and VRAM temps
            gpuMaxTemperatureKeyOffset = 164;
            memoryMaxTemperatureKeyOffset = 168;
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
                memcpy(buf, validGuid1.data(), count); // Always return 0x5e2f8210 for BMG
            }
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &gpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTemp0Offset) {
                memcpy(buf, &vrTemperature0, count);
            } else if (offset == vrTemp1Offset) {
                memcpy(buf, &vrTemperature1, count);
            } else if (offset == vrTemp2Offset) {
                memcpy(buf, &vrTemperature2, count);
            } else if (offset == vrTemp3Offset) {
                memcpy(buf, &vrTemperature3, count);
            } else if (offset == gpuBoardTempOffset) {
                memcpy(buf, &gpuBoardTemperature, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // BMG only checks GPU and Memory: max(10, 20) = 20
    // CRI checks GPU, Memory, VR, and Board: max(10, 20, 35, 40) = 40
    if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
        EXPECT_EQ(40.0, temperature);
    } else {
        EXPECT_EQ(20.0, temperature);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndGetVoltageRegulatorTemperatureFailsThenFailureIsReturned, IsCRI) {
    static uint32_t gpuMaxTemperature = 10;
    static uint32_t memoryMaxTemperature = 20;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        static int guidReadCount = 0;
        uint64_t telemOffset = 0;
        std::string validGuid1 = "";
        std::string validGuid2 = "";
        long gpuMaxTemperatureKeyOffset = 0;
        long memoryMaxTemperatureKeyOffset = 0;
        long vrTemp0Offset = 0;

        validGuid1 = "0x1e2fa030";
        validGuid2 = "0x5e2fa230";
        gpuMaxTemperatureKeyOffset = 128;
        memoryMaxTemperatureKeyOffset = 132;
        vrTemp0Offset = 200;

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
                memcpy(buf, &gpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTemp0Offset) {
                // Fail VR temperature read
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGlobalMaxTemperatureAndGetGpuBoardTemperatureFailsThenFailureIsReturned, IsCRI) {
    static uint32_t gpuMaxTemperature = 10;
    static uint32_t memoryMaxTemperature = 20;
    static uint32_t vrTemperature0 = 150;
    static uint32_t vrTemperature1 = 25;
    static uint32_t vrTemperature2 = 30;
    static uint32_t vrTemperature3 = 35;

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
        long gpuBoardTempOffset = 0;

        validGuid1 = "0x1e2fa030";
        validGuid2 = "0x5e2fa230";
        gpuMaxTemperatureKeyOffset = 128;
        memoryMaxTemperatureKeyOffset = 132;
        vrTemp0Offset = 200;
        vrTemp1Offset = 204;
        vrTemp2Offset = 208;
        vrTemp3Offset = 212;
        gpuBoardTempOffset = 176;

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
                memcpy(buf, &gpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTemp0Offset) {
                memcpy(buf, &vrTemperature0, count);
            } else if (offset == vrTemp1Offset) {
                memcpy(buf, &vrTemperature1, count);
            } else if (offset == vrTemp2Offset) {
                memcpy(buf, &vrTemperature2, count);
            } else if (offset == vrTemp3Offset) {
                memcpy(buf, &vrTemperature3, count);
            } else if (offset == gpuBoardTempOffset) {
                // Fail GPU board temperature read
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
    static uint32_t gpuMaxTemperature = 10;
    static uint32_t memoryMaxTemperature = 20;
    static uint32_t vrTemperature0 = 150;
    static uint32_t vrTemperature1 = 25;
    static uint32_t vrTemperature2 = 30;
    static uint32_t vrTemperature3 = 35;
    static uint32_t gpuBoardTemperature = 0x001E0028;
    uint32_t validTemperatureHandleCount = 3u;

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
        long vrTemp0Offset = 0, vrTemp1Offset = 0, vrTemp2Offset = 0, vrTemp3Offset = 0;
        long gpuBoardTempOffset = 0;

        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            validGuid1 = "0x1e2fa030";
            validGuid2 = "0x5e2fa230";
            gpuMaxTemperatureKeyOffset = 128;
            memoryMaxTemperatureKeyOffset = 132;
            vrTemp0Offset = 200;
            vrTemp1Offset = 204;
            vrTemp2Offset = 208;
            vrTemp3Offset = 212;
            gpuBoardTempOffset = 176;
        } else {
            // BMG has all temperature data in one GUID: 0x5e2f8210
            validGuid1 = "0x5e2f8210";
            validGuid2 = "0x5e2f8210";
            gpuMaxTemperatureKeyOffset = 164;
            memoryMaxTemperatureKeyOffset = 168;
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
                memcpy(buf, validGuid1.data(), count); // Always return 0x5e2f8210 for BMG
            }
        } else if (fd == 6) {
            if (offset == gpuMaxTemperatureKeyOffset) {
                memcpy(buf, &gpuMaxTemperature, count);
            } else if (offset == memoryMaxTemperatureKeyOffset) {
                memcpy(buf, &memoryMaxTemperature, count);
            } else if (offset == vrTemp0Offset) {
                memcpy(buf, &vrTemperature0, count);
            } else if (offset == vrTemp1Offset) {
                memcpy(buf, &vrTemperature1, count);
            } else if (offset == vrTemp2Offset) {
                memcpy(buf, &vrTemperature2, count);
            } else if (offset == vrTemp3Offset) {
                memcpy(buf, &vrTemperature3, count);
            } else if (offset == gpuBoardTempOffset) {
                memcpy(buf, &gpuBoardTemperature, count);
            }
        }
        return count;
    });

    uint32_t count = 0;
    if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
        validTemperatureHandleCount = 9u;
    }
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
            // BMG: max(gpu=10, mem=20) = 20
            // CRI: max(gpu=10, mem=20, vr=35, board=40) = 40
            if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
                EXPECT_EQ(40.0, temperature);
            } else {
                EXPECT_EQ(static_cast<double>(std::max(gpuMaxTemperature, memoryMaxTemperature)), temperature);
            }
        }
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

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGlobalMaxTemperatureThenFailureIsReturned, IsCRI) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGlobalMaxTemperature(pLinuxSysmanImp, &temperature, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingVRTemperatureThenFailureIsReturned, IsCRI) {
    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenGettingGpuBoardTemperatureThenFailureIsReturned, IsCRI) {
    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingVRTemperatureThenFailureIsReturned, IsCRI) {
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
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadOffsetFailsFromPmtUtilWhenGettingGpuBoardTemperatureThenFailureIsReturned, IsCRI) {
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
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndReadGuidFailsFromPmtUtilWhenGettingVRTemperatureThenFailureIsReturned, IsCRI) {
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
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
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
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenGettingVRTemperatureThenFailureIsReturned, IsCRI) {
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
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
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
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingVRTemperatureThenValidValueIsReturned, IsCRI) {
    static uint32_t mockVrTemperature[4] = {450, 50, 55, 60};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 200;
        long vr1Offset = 204;
        long vr2Offset = 208;
        long vr3Offset = 212;
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

    // Test VR temperatures
    for (uint32_t sensorIndex = 0; sensorIndex < 4; sensorIndex++) {
        double temperature = 0;
        ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        double expectedTemp = (sensorIndex == 0) ? static_cast<double>(mockVrTemperature[sensorIndex] / 10)
                                                 : static_cast<double>(mockVrTemperature[sensorIndex] & 0xFF);
        EXPECT_EQ(expectedTemp, temperature);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGpuBoardTemperatureThenValidValueIsReturned, IsCRI) {
    static uint32_t mockGpuBoardTempRegister = 0x00320000 | 0x0028; // Board1=50 (0x32), Board0=40 (0x28)
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long gpuBoardOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuBoardOffset) {
                memcpy(buf, &mockGpuBoardTempRegister, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    // Test GPU board temperatures
    for (uint32_t sensorIndex = 0; sensorIndex < 2; sensorIndex++) {
        double temperature = 0;
        ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        double expectedTemp = (sensorIndex == 0) ? 40.0 : 50.0; // 0x28=40, 0x32=50
        EXPECT_EQ(expectedTemp, temperature);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingVRTemperatureFailsThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 200;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                count = -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingGpuBoardTemperatureFailsThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long gpuBoardOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuBoardOffset) {
                count = -1;
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingVRTemperatureWithInvalidSensorIndexThenValueIsReturnedWithoutTransformationAlongWithError, IsCRI) {
    static uint32_t mockVrTemperature = 0;

    auto guidToKeyOffsetMapBackup = const_cast<std::map<std::string, std::map<std::string, uint64_t>> *>(
        L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily)->getGuidToKeyOffsetMap());
    std::map<std::string, std::map<std::string, uint64_t>> originalMap = *guidToKeyOffsetMapBackup;
    (*guidToKeyOffsetMapBackup)["0x1e2fa030"]["VR_TEMPERATURE_4"] = 216;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr4Offset = 216;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr4Offset) {
                memcpy(buf, &mockVrTemperature, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 4;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    *guidToKeyOffsetMapBackup = originalMap;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenGettingGpuBoardTemperatureWithInvalidSensorIndexThenValueIsReturnedWithoutTransformationAlongWithError, IsCRI) {
    static uint32_t mockGpuBoardTemperature = 0;

    auto guidToKeyOffsetMapBackup = const_cast<std::map<std::string, std::map<std::string, uint64_t>> *>(
        L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily)->getGuidToKeyOffsetMap());
    std::map<std::string, std::map<std::string, uint64_t>> originalMap = *guidToKeyOffsetMapBackup;
    (*guidToKeyOffsetMapBackup)["0x1e2fa030"]["GPU_BOARD_TEMPERATURE_2"] = 180;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long gpuBoard2Offset = 180;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuBoard2Offset) {
                memcpy(buf, &mockGpuBoardTemperature, count);
            }
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 2;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    *guidToKeyOffsetMapBackup = originalMap;
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenZesGetTemperatureStateIsCalledForVRThenValidTemperatureValueIsReturned, IsCRI) {
    static uint32_t vr0Temperature = 450;
    static uint32_t vr1Temperature = 48;
    static uint32_t vr2Temperature = 43;
    static uint32_t vr3Temperature = 50;
    static uint32_t validTemperatureHandleCount = 9u;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long vr0Offset = 200;
        long vr1Offset = 204;
        long vr2Offset = 208;
        long vr3Offset = 212;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == vr0Offset) {
                memcpy(buf, &vr0Temperature, count);
            } else if (offset == vr1Offset) {
                memcpy(buf, &vr1Temperature, count);
            } else if (offset == vr2Offset) {
                memcpy(buf, &vr2Temperature, count);
            } else if (offset == vr3Offset) {
                memcpy(buf, &vr3Temperature, count);
            } else {
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

    uint32_t vrCount = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;

        if (properties.type == ZES_TEMP_SENSORS_VOLTAGE_REGULATOR) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            vrCount++;
            EXPECT_TRUE(temperature == static_cast<double>(vr0Temperature / 10) ||
                        temperature == static_cast<double>(vr1Temperature & 0xFF) ||
                        temperature == static_cast<double>(vr2Temperature & 0xFF) ||
                        temperature == static_cast<double>(vr3Temperature & 0xFF));
        } else if (properties.type == ZES_TEMP_SENSORS_GLOBAL || properties.type == ZES_TEMP_SENSORS_GPU) {
            EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesTemperatureGetState(handle, &temperature));
        }
    }
    EXPECT_EQ(vrCount, 4u);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenZesGetTemperatureStateIsCalledForGpuBoardThenValidTemperatureValueIsReturned, IsCRI) {
    static uint32_t gpuBoardTempRegister = 0x00370000 | 0x002E; // Board1=55 (0x37), Board0=46 (0x2E)
    static uint32_t validTemperatureHandleCount = 9u;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x1e2fa030";
        long gpuBoardOffset = 176;
        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            if (offset == gpuBoardOffset) {
                memcpy(buf, &gpuBoardTempRegister, count);
            } else {
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

    uint32_t gpuBoardCount = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;

        if (properties.type == ZES_TEMP_SENSORS_GPU_BOARD) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            gpuBoardCount++;
            // Expect either board sensor 0 (0x2E = 46) or board sensor 1 (0x37 = 55)
            EXPECT_TRUE(temperature == 46.0 || temperature == 55.0);
        } else if (properties.type == ZES_TEMP_SENSORS_GLOBAL || properties.type == ZES_TEMP_SENSORS_GPU) {
            EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesTemperatureGetState(handle, &temperature));
        }
    }
    EXPECT_EQ(gpuBoardCount, 2u);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingVRTemperatureOnAnUnsupportedPlatformThenErrorIsReturned, IsAtMostBMG) {
    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getVoltageRegulatorTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenSysmanProductHelperInstanceWhenReadingGpuBoardTemperatureOnAnUnsupportedPlatformThenErrorIsReturned, IsAtMostBMG) {
    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double temperature = 0;
    ze_result_t result = pSysmanProductHelper->getGpuBoardTemperature(pLinuxSysmanImp, &temperature, subdeviceId, sensorIndex);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
