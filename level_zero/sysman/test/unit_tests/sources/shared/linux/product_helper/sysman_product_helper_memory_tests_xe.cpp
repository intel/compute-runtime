/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

#include <bit>

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperMemoryXeTest = SysmanDeviceFixture;

constexpr uint32_t mockOffsetNumChannelsPerMsu = 3660;
constexpr uint32_t mockOffsetMsuBitmask = 3688;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {
    std::map<std::string, std::string> fileNameLinkMap = {
        {telem1NodeName, "../../devices/pci0000:00/0000:00:0a.0/intel-vsec.telemetry.0/intel_pmt/telem1/"},
        {telem2NodeName, "../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0/intel-vsec.telemetry.1/intel_pmt/telem2/"}};
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        if (bufsize == 0) {
            return -1;
        }
        size_t copySize = (bufsize - 1 < it->second.size()) ? (bufsize - 1) : it->second.size();
        std::memcpy(buf, it->second.c_str(), copySize);
        buf[copySize] = '\0';
        return static_cast<int>(copySize);
    }
    return -1;
}

static int mockOpenSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem1OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem1GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem1TelemFileName) {
        returnValue = 6;
    } else if (strPathName == telem2OffsetFileName) {
        returnValue = 7;
    } else if (strPathName == telem2GuidFileName) {
        returnValue = 8;
    } else if (strPathName == telem2TelemFileName) {
        returnValue = 9;
    }
    return returnValue;
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryPropertiesThenVerifyCallSucceeds, IsWithinXeCoreAndXe2HpgCore) {
    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;

    auto pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    MockMemoryFsAccessInterface *pFsAccess = new MockMemoryFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pLinuxSysmanImp->pFsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getFsAccess();
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(properties.location, ZES_MEM_LOC_SYSTEM);
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_EQ(properties.busWidth, -1);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_FORCE_UINT32);
        EXPECT_EQ(properties.physicalSize, mockIntegratedDevicePhysicalSize);
    } else {
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_EQ(properties.numChannels, numMemoryChannels);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, strtoull(mockPhysicalSize.c_str(), nullptr, 16));
    }
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndNoTelemNodesAvailableThenFailureIsReturned, IsBMG) {
    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth;
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndReadGuidFailsFromPmtUtilThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5 || fd == 8) {
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth;
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndReadOffsetFailsFromPmtUtilThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string dummyOobmsmGuid = "0xABCDEF";
        if (fd == 4 || fd == 8) {
            return -1;
        } else if (fd == 5) {
            memcpy(buf, dummyOobmsmGuid.data(), count);
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth;
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndKeyOffsetMapIsNotAvailableThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem2Offset = 0;
        std::string dummyOobmsmGuid = "0xABCDEF";
        if (fd == 4) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 5) {
            memcpy(buf, dummyOobmsmGuid.data(), count);
        } else if (fd == 8) {
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth;
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndReadValueFailsForOobmsmPathKeysThenFailureIsReturned, IsBMG) {
    static int readFailCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem2Offset = 0;
        uint32_t mockMsuBitMask = 0x3;
        std::string validOobmsmGuid = "0x5e2f8210";
        if (fd == 4) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6) {
            switch (offset) {
            case 376:
                return (readFailCount == 2) ? -1 : sizeof(uint32_t);
            case 380:
                return (readFailCount == 3) ? -1 : sizeof(uint32_t);
            case 392:
                return (readFailCount == 4) ? -1 : sizeof(uint32_t);
            case 396:
                return (readFailCount == 5) ? -1 : sizeof(uint32_t);
            case 3688:
                memcpy(buf, &mockMsuBitMask, count);
                if (readFailCount == 1) {
                    return -1;
                }
                break;
            }
        } else if (fd == 8) {
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth;
    while (readFailCount <= 5) {
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId));
        readFailCount++;
    }
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndReadValueFailsForPunitPathKeyThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        uint64_t telem2Offset = 0;
        uint32_t mockKeyValue = 0x3;
        std::string validOobmsmGuid = "0x5e2f8210";
        std::string validPunitGuid = "0x1e2f8200";
        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &mockKeyValue, count);
        } else if (fd == 7) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 8) {
            memcpy(buf, validPunitGuid.data(), count);
        } else if (fd == 9) {
            return -1;
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId));
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthThenValidValuesAreReturned, IsBMG) {
    static uint32_t readCounterUpper32Bit = 0x3;
    static uint32_t readCounterLower32Bit = 0xc;
    static uint32_t readCounterUpper64Bit = 0x4;
    static uint32_t readCounterLower64Bit = 0xd;
    static uint32_t writeCounterUpper32Bit = 0xe;
    static uint32_t writeCounterLower32Bit = 0xa;
    static uint32_t writeCounterUpper64Bit = 0xf;
    static uint32_t writeCounterLower64Bit = 0xb;
    static uint32_t vramBandwidth = 0x6abc0000;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem2Offset = 0;
        uint64_t telem3Offset = 0;
        std::string validOobmsmGuid = "0x5e2f8210";
        std::string validPunitGuid = "0x1e2f8202";
        uint32_t mockMsuBitMask = 0x3;
        uint32_t mockKeyValue = 0;

        if (fd == 4) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6) {
            switch (offset) {
            case 376:
                memcpy(buf, &readCounterUpper32Bit, count);
                break;
            case 380:
                memcpy(buf, &readCounterLower32Bit, count);
                break;
            case 384:
                memcpy(buf, &readCounterUpper64Bit, count);
                break;
            case 388:
                memcpy(buf, &readCounterLower64Bit, count);
                break;
            case 392:
                memcpy(buf, &writeCounterUpper32Bit, count);
                break;
            case 396:
                memcpy(buf, &writeCounterLower32Bit, count);
                break;
            case 400:
                memcpy(buf, &writeCounterUpper64Bit, count);
                break;
            case 404:
                memcpy(buf, &writeCounterLower64Bit, count);
                break;
            case 3688:
                memcpy(buf, &mockMsuBitMask, count);
                break;
            default:
                memcpy(buf, &mockKeyValue, count);
                break;
            }
        } else if (fd == 7) {
            memcpy(buf, &telem3Offset, count);
        } else if (fd == 8) {
            memcpy(buf, validPunitGuid.data(), count);
        } else if (fd == 9) {
            memcpy(buf, &vramBandwidth, count);
        }
        return count;
    });

    uint32_t subdeviceId = 0;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId));

    uint64_t outputReadCounter32Bit = packInto64Bit(readCounterUpper32Bit, readCounterLower32Bit) * 32;
    uint64_t outputReadCounter64Bit = packInto64Bit(readCounterUpper64Bit, readCounterLower64Bit) * 64;
    uint64_t outputWriteCounter32Bit = packInto64Bit(writeCounterUpper32Bit, writeCounterLower32Bit) * 32;
    uint64_t outputWriteCounter64Bit = packInto64Bit(writeCounterUpper64Bit, writeCounterLower64Bit) * 64;
    uint64_t outputMaxBandwidth = static_cast<uint64_t>(vramBandwidth >> 16);

    EXPECT_EQ((outputReadCounter32Bit + outputReadCounter64Bit), memBandwidth.readCounter);
    EXPECT_EQ((outputWriteCounter32Bit + outputWriteCounter64Bit), memBandwidth.writeCounter);
    EXPECT_GT(memBandwidth.timestamp, 0u);
    EXPECT_EQ(outputMaxBandwidth * mbpsToBytesPerSec * 100, memBandwidth.maxBandwidth);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetNumberOfMemoryChannelsAndTelemNodesAreNotAvailableThenErrorIsReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t numChannels = 0;

    ze_result_t result = pSysmanProductHelper->getNumberOfMemoryChannels(pLinuxSysmanImp, &numChannels);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetNumberOfMemoryChannelsAndReadGuidFailsThenErrorIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5 || fd == 8) {
            return -1;
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t numChannels = 0;

    ze_result_t result = pSysmanProductHelper->getNumberOfMemoryChannels(pLinuxSysmanImp, &numChannels);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetNumberOfMemoryChannelsAndGuidNotFoundInMapThenErrorIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem2Offset = 0;
        std::string invalidGuid = "0x12345678"; // This GUID doesn't exist in BMG's guidToKeyOffsetMap

        if (fd == 4 || fd == 7) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 5 || fd == 8) {
            memcpy(buf, invalidGuid.data(), count);
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t numChannels = 0;

    ze_result_t result = pSysmanProductHelper->getNumberOfMemoryChannels(pLinuxSysmanImp, &numChannels);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetNumberOfMemoryChannelsAndKeyOffsetMapIsEmptyThenErrorIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem2Offset = 0;
        std::string validButEmptyGuid = "0x1e2f8200"; // Valid GUID but no NUM_OF_MEM_CHANNEL key

        if (fd == 4 || fd == 7) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 5 || fd == 8) {
            memcpy(buf, validButEmptyGuid.data(), count);
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t numChannels = 0;

    ze_result_t result = pSysmanProductHelper->getNumberOfMemoryChannels(pLinuxSysmanImp, &numChannels);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetNumberOfMemoryChannelsAndReadValueFailsThenErrorIsReturned, IsBMG) {
    static int readFailCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem2Offset = 0;
        std::string validOobmsmGuid = "0x5e2f8210";

        if (fd == 4 || fd == 7) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 5 || fd == 8) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6 || fd == 9) {
            switch (offset) {
            case mockOffsetNumChannelsPerMsu:
                count = (readFailCount == 1) ? -1 : sizeof(uint32_t);
                break;
            case mockOffsetMsuBitmask:
                count = (readFailCount == 2) ? -1 : sizeof(uint32_t);
                break;
            }
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t numChannels = 0;

    while (readFailCount <= 2) {
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getNumberOfMemoryChannels(pLinuxSysmanImp, &numChannels));
        readFailCount++;
    }
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryPropertiesWithGddr6AndGetNumberOfMemoryChannelsSucceedsThenValidPropertiesAreReturned, IsBMG) {
    static uint32_t mockMsuBitMask = 0b111111;
    static uint32_t numChannelPerMsu = 2;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem2Offset = 0;
        std::string validOobmsmGuid = "0x5e2f8311";

        if (fd == 4 || fd == 7) {
            memcpy(buf, &telem2Offset, count);
        } else if (fd == 5 || fd == 8) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6 || fd == 9) {
            switch (offset) {
            case mockOffsetNumChannelsPerMsu:
                memcpy(buf, &numChannelPerMsu, count);
                break;
            case mockOffsetMsuBitmask:
                memcpy(buf, &mockMsuBitMask, count);
                break;
            }
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::gddr6);

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_GDDR6);
    uint32_t totalNumberOfMsus = std::popcount(mockMsuBitMask);
    int32_t expectedNumChannels = static_cast<int32_t>(numChannelPerMsu * totalNumberOfMsus);
    EXPECT_EQ(properties.numChannels, expectedNumChannels);
    EXPECT_EQ(properties.busWidth, properties.numChannels * 32);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryPropertiesWithGddr6AndGetNumberOfMemoryChannelsFailsThenDefaultValuesAreReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::gddr6);

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_GDDR6);
    EXPECT_EQ(properties.numChannels, -1);
    EXPECT_EQ(properties.busWidth, -1);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetNumberOfMemoryChannelsAndDeviceIsNotBMGThenErrorIsReturned, IsNotBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t numChannels = 0;
    ze_result_t result = pSysmanProductHelper->getNumberOfMemoryChannels(pLinuxSysmanImp, &numChannels);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenCallingGetMemoryBandwidthThenErrorIsReturned, IsAtLeastXe2HpgCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth = {};
    uint32_t subdeviceId = 0;

    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndReadGuidFailsWhenCallingGetMemoryBandwidthThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth = {};
    uint32_t subdeviceId = 0;

    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenCallingGetMemoryBandwidthThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string invalidGuid = "0xABCDEF";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, invalidGuid.data(), count);
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth = {};
    uint32_t subdeviceId = 0;

    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndReadValueFailsForMemoryBandwidthCountersWhenCallingGetMemoryBandwidthThenErrorIsReturned, IsCRI) {
    static int readFailCount = 1;
    static uint64_t mb0ReadCounterValue = 100;
    static uint64_t mb1ReadCounterValue = 200;
    static uint64_t mb0WriteCounterValue = 300;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x5e2fa230";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            switch (offset) {
            case 688:
                if (readFailCount == 1) {
                    return -1;
                }
                memcpy(buf, &mb0ReadCounterValue, count);
                break;
            case 728:
                if (readFailCount == 2) {
                    return -1;
                }
                memcpy(buf, &mb1ReadCounterValue, count);
                break;
            case 680:
                if (readFailCount == 3) {
                    return -1;
                }
                memcpy(buf, &mb0WriteCounterValue, count);
                break;
            case 720:
                if (readFailCount == 4) {
                    return -1;
                }
                break;
            }
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth = {};
    uint32_t subdeviceId = 0;

    while (readFailCount <= 4) {
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId));
        readFailCount++;
    }
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndReadValueFailsForVramBandwidthWhenCallingGetMemoryBandwidthThenErrorIsReturned, IsCRI) {

    static uint64_t readCounterValue = 1000000;
    static uint64_t writeCounterValue = 2000000;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validOobsmGuid = "0x5e2fa230";
        std::string validPunitGuid = "0x1e2fa030";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobsmGuid.data(), count);
        } else if (fd == 6) {
            switch (offset) {
            case 688:
                memcpy(buf, &readCounterValue, count);
                break;
            case 680:
                memcpy(buf, &writeCounterValue, count);
                break;
            default:
                uint64_t zeroValue = 0;
                memcpy(buf, &zeroValue, count);
                break;
            }
        } else if (fd == 8) {
            memcpy(buf, validPunitGuid.data(), count);
        } else if (fd == 9) {
            if (offset == 56) {
                return -1;
            } else {
                uint64_t zeroValue = 0;
                memcpy(buf, &zeroValue, count);
            }
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth = {};
    uint32_t subdeviceId = 0;

    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthThenValidValuesAreReturned, IsCRI) {

    static uint64_t readCounterValue = 1000000;
    static uint64_t writeCounterValue = 2000000;
    static uint32_t vramBandwidth = 0x6abc0000;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validOobsmGuid = "0x5e2fa230";
        std::string validPunitGuid = "0x1e2fa030";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobsmGuid.data(), count);
        } else if (fd == 6) {
            switch (offset) {
            case 688:
                memcpy(buf, &readCounterValue, count);
                break;
            case 680:
                memcpy(buf, &writeCounterValue, count);
                break;
            default:
                uint64_t zeroValue = 0;
                memcpy(buf, &zeroValue, count);
                break;
            }
        } else if (fd == 8) {
            memcpy(buf, validPunitGuid.data(), count);
        } else if (fd == 9) {
            if (offset == 56) {
                memcpy(buf, &vramBandwidth, count);
            } else {
                uint64_t zeroValue = 0;
                memcpy(buf, &zeroValue, count);
            }
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t memBandwidth = {};
    uint32_t subdeviceId = 0;
    uint64_t transactionSize = 64;
    uint64_t expectedReadCounter = readCounterValue * transactionSize;
    uint64_t expectedWriteCounter = writeCounterValue * transactionSize;
    uint64_t expectedMaxBandwidth = static_cast<uint64_t>(vramBandwidth >> 16) * mbpsToBytesPerSec;
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&memBandwidth, pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(memBandwidth.readCounter, expectedReadCounter);
    EXPECT_EQ(memBandwidth.writeCounter, expectedWriteCounter);
    EXPECT_EQ(memBandwidth.maxBandwidth, expectedMaxBandwidth);
    EXPECT_GT(memBandwidth.timestamp, 0u);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndNoTelemNodesAvailableWhenCallingGetMemoryPropertiesThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndReadGuidFailsWhenCallingGetMemoryPropertiesThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndKeyOffsetMapIsNotAvailableWhenCallingGetMemoryPropertiesThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string invalidGuid = "0xABCDEF";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, invalidGuid.data(), count);
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceAndReadNumChannelsFailsWhenCallingGetMemoryPropertiesThenErrorIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x5e2fa230";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            return -1;
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryXeTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryPropertiesThenValidPropertiesAreReturned, IsCRI) {
    static const uint32_t numChannelPerMsu = 4;
    static const uint32_t memoryMsuCount = 20;
    static const uint32_t busWidthPerChannelInBytes = 2;

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x5e2fa230";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &numChannelPerMsu, count);
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = true;
    uint32_t subDeviceId = 1;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    const int32_t expectedNumChannels = static_cast<int32_t>(numChannelPerMsu * memoryMsuCount);
    const int32_t expectedBusWidth = expectedNumChannels * busWidthPerChannelInBytes;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_EQ(properties.type, static_cast<zes_mem_type_t>(ZES_INTEL_MEM_TYPE_LPDDR5X));
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, subDeviceId);
    EXPECT_EQ(properties.numChannels, expectedNumChannels);
    EXPECT_EQ(properties.busWidth, expectedBusWidth);
    EXPECT_EQ(properties.physicalSize, 0u);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
