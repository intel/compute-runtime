/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperMemoryTest = SysmanDeviceFixture;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {
    std::map<std::string, std::string> fileNameLinkMap = {
        {telem1NodeName, "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
        {telem2NodeName, "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        {telem3NodeName, "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem3/"}};
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        if (bufsize == 0) {
            return -1;
        }
        size_t copySize = std::min(bufsize - 1, it->second.size());
        std::memcpy(buf, it->second.c_str(), copySize);
        buf[copySize] = '\0';
        return static_cast<int>(copySize);
    }
    return -1;
}

static int mockReadLinkSingleTelemetryNodeSuccess(const char *path, char *buf, size_t bufsize) {
    std::map<std::string, std::string> fileNameLinkMap = {
        {telem1NodeName, "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        if (bufsize == 0) {
            return -1;
        }
        size_t copySize = std::min(bufsize - 1, it->second.size());
        std::memcpy(buf, it->second.c_str(), copySize);
        buf[copySize] = '\0';
        return static_cast<int>(copySize);
    }
    return -1;
}

static int mockOpenSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem2OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem2GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem2TelemFileName) {
        returnValue = 6;
    } else if (strPathName == hbmFreqFilePath) {
        returnValue = 7;
    } else if (strPathName == telem3OffsetFileName) {
        returnValue = 8;
    } else if (strPathName == telem3GuidFileName) {
        returnValue = 9;
    } else if (strPathName == telem3TelemFileName) {
        returnValue = 10;
    } else if (strPathName == hbmFreqFilePath2) {
        returnValue = 11;
    } else if (strPathName == telem1OffsetFileName) {
        returnValue = 12;
    } else if (strPathName == telem1GuidFileName) {
        returnValue = 13;
    } else if (strPathName == telem1TelemFileName) {
        returnValue = 14;
    }
    return returnValue;
}

static int mockOpenSingleTelemetryNodeSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem1OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem1GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem1TelemFileName) {
        returnValue = 6;
    } else if (strPathName == maxBwFileName) {
        returnValue = 7;
    }
    return returnValue;
}

static ssize_t mockReadSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint32_t val = 0;
    if (fd == 4) {
        oStream << "0";
    } else if (fd == 5) {
        oStream << "0xb15a0edd";
    } else if (fd == 6) {
        if (offset == vF0Vfid) {
            val = 1;
        } else if (offset == vF0Hbm0Read) {
            val = vFHbm0ReadValue;
        } else if (offset == vF0Hbm0Write) {
            val = vFHbm0WriteValue;
        } else if (offset == vF0Hbm1Read) {
            val = vFHbm1ReadValue;
        } else if (offset == vF0Hbm1Write) {
            val = vFHbm1WriteValue;
        } else if (offset == vF0Hbm2Read) {
            val = vFHbm2ReadValue;
        } else if (offset == vF0Hbm2Write) {
            val = vFHbm2WriteValue;
        } else if (offset == vF0Hbm3Read) {
            val = vFHbm3ReadValue;
        } else if (offset == vF0Hbm3Write) {
            val = vFHbm3WriteValue;
        }
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 7) {
        oStream << hbmRP0Frequency;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadSuccess64BitRead(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint32_t val = 0;
    if (fd == 4 || fd == 8) {
        oStream << "0";
    } else if (fd == 5 || fd == 9) {
        oStream << "0xb15a0ede";
    } else if (fd == 6 || fd == 10) {
        if (offset == vF1Vfid) {
            val = 1;
        } else if (offset == vF1HbmReadL) {
            val = vFHbmLRead;
        } else if (offset == vF1HbmReadH) {
            val = vFHbmHRead;
        } else if (offset == vF1HbmWriteL) {
            val = vFHbmLWrite;
        } else if (offset == vF1HbmWriteH) {
            val = vFHbmHWrite;
        }
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 7 || fd == 11) {
        oStream << hbmRP0Frequency;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadIdiFilesSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint64_t val = 0;
    if (fd == 4) {
        oStream << "0";
    } else if (fd == 5) {
        oStream << "0x4f9502";
    } else if (fd == 6) {
        if (offset >= minIdiReadOffset && offset < minIdiWriteOffset) {
            val = mockIdiReadVal;
        } else if (offset >= minIdiWriteOffset && offset < minDisplayVc1ReadOffset) {
            val = mockIdiWriteVal;
        } else if (offset >= minDisplayVc1ReadOffset) {
            val = mockDisplayVc1ReadVal;
        }
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 7) {
        oStream << mockMaxBwDg2;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static void setupMockDrmWithMemoryType(L0::Sysman::SysmanDeviceImp *pSysmanDeviceImp, int memoryType) {
    debugManager.flags.EnableLocalMemory.set(1);
    auto pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));
    pDrm->setMemoryType(memoryType);
    pDrm->ioctlHelper = std::make_unique<NEO::MockIoctlHelper>(*pDrm);
}

static std::vector<zes_mem_handle_t> getMemoryHandles(zes_device_handle_t device) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumMemoryModules(device, &count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    result = zesDeviceEnumMemoryModules(device, &count, handles.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    return handles;
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndReadGuidFailsThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;
    zes_mem_bandwidth_t bandwidth = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndVfid0IsActiveThenVerifyBandWidthIsValid, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t expectedReadCounters = vFHbm0ReadValue + vFHbm1ReadValue + vFHbm2ReadValue + vFHbm3ReadValue;
    uint64_t expectedWriteCounters = vFHbm0WriteValue + vFHbm1WriteValue + vFHbm2WriteValue + vFHbm3WriteValue;
    uint64_t expectedBandwidth = 0;

    expectedReadCounters = expectedReadCounters * transactionSize;
    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    expectedWriteCounters = expectedWriteCounters * transactionSize;
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndVfid1IsActiveThenVerifyBandWidthIsValid, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess64BitRead);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
    expectedReadCounters |= vFHbmHRead;
    expectedReadCounters = (expectedReadCounters << 32) | vFHbmLRead;
    expectedReadCounters = expectedReadCounters * transactionSize;
    expectedWriteCounters |= vFHbmHWrite;
    expectedWriteCounters = (expectedWriteCounters << 32) | vFHbmLWrite;
    expectedWriteCounters = expectedWriteCounters * transactionSize;
    uint64_t expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;

    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthTwiceAndVfid1IsActiveThenVerifyBandWidthIsValid, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess64BitRead);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth = {};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
    expectedReadCounters |= vFHbmHRead;
    expectedReadCounters = (expectedReadCounters << 32) | vFHbmLRead;
    expectedReadCounters = expectedReadCounters * transactionSize;
    expectedWriteCounters |= vFHbmHWrite;
    expectedWriteCounters = (expectedWriteCounters << 32) | vFHbmLWrite;
    expectedWriteCounters = expectedWriteCounters * transactionSize;
    uint64_t expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;

    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);

    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthForEachSubdeviceAndVfid1IsActiveThenVerifyBandWidthIsValid, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess64BitRead);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth = {};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
    expectedReadCounters |= vFHbmHRead;
    expectedReadCounters = (expectedReadCounters << 32) | vFHbmLRead;
    expectedReadCounters = expectedReadCounters * transactionSize;
    expectedWriteCounters |= vFHbmHWrite;
    expectedWriteCounters = (expectedWriteCounters << 32) | vFHbmLWrite;
    expectedWriteCounters = expectedWriteCounters * transactionSize;
    uint64_t expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;

    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);

    subDeviceId += 1;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenBothVfid0AndVfid1ActiveAndGuidNotSetWhenCallingGetBandwidthThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = -1;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0edd";
        } else if (fd == 6) {
            if (offset == vF0Vfid || offset == vF1Vfid) {
                val = 1;
            }
            memcpy(buf, &val, sizeof(uint32_t));
            return sizeof(uint32_t);
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;
    zes_mem_bandwidth_t bandwidth = {};
    EXPECT_EQ(pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenBothVfid0AndVfid1NotActiveAndGuidNotSetWhenCallingGetBandwidthThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = -1;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0edd";
        } else if (fd == 6) {
            if (offset == vF0Vfid || offset == vF1Vfid) {
                val = 0;
            }
            memcpy(buf, &val, sizeof(uint32_t));
            return sizeof(uint32_t);
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;
    zes_mem_bandwidth_t bandwidth = {};
    EXPECT_EQ(pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0ReadFailsAndGuidNotSetWhenCallingGetBandwidthThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0edd";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
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

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth = {};
    EXPECT_EQ(pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0ActiveAndVfid1ReadFailsAndGuidNotSetWhenCallingGetBandwidthThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = -1;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0edd";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
                val = 1;
            } else if (offset == vF1Vfid) {
                errno = ENOENT;
                return -1;
            }
            memcpy(buf, &val, sizeof(uint32_t));
            return sizeof(uint32_t);
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth = {};
    EXPECT_EQ(pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenHbm0ReadFailsAndGuidNotSetWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0edd";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
                val = 1;
            } else if (offset == vF0Hbm0Read) {
                errno = ENOENT;
                return -1;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenHbm0WriteFailsAndGuidNotSetWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0edd";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
                val = 1;
            } else if (offset == vF0Hbm0Write) {
                errno = ENOENT;
                return -1;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0ReadLFailsWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
                val = 1;
            } else if (offset == vF0HbmReadL) {
                return -1;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0ReadHFailsWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
                val = 1;
            } else if (offset == vF0HbmReadH) {
                return -1;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0WriteLFailsWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
                val = 1;
            } else if (offset == vF0HbmWriteL) {
                return -1;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0WriteHFailsWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == vF0Vfid) {
                val = 1;
            } else if (offset == vF0HbmWriteH) {
                return -1;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenInvalidGuidWhenCallingGetBandwidthThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndReadGuidFailsThenErrorIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;
    zes_mem_bandwidth_t bandwidth = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenInvalidGuidWhenCallingGetBandwidthThenFailureIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xABCDEF";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAPIsThenErrorIsReturned, IsDG1) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, -1);
    EXPECT_EQ(properties.busWidth, -1);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWithPrelimInterfaceWhenCallingGetMemoryPropertiesThenVerifyPropertiesAreProper, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pSysmanKmdInterface, subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, strtoull(mockPhysicalSize.c_str(), nullptr, 16));
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceAndPhysicalSizeReadFailsWhenCallingGetMemoryPropertiesThenVerifyPhysicalSizeIsZero, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pSysfsAccess->mockReadStringValue.push_back("0");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pSysmanKmdInterface, subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.physicalSize, 0u);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenSuccessIsReturned, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadIdiFilesSuccess);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);

    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    expectedReadCounters = numberMcChannels * (mockIdiReadVal + mockDisplayVc1ReadVal) * transactionSize;
    EXPECT_EQ(expectedReadCounters, bandwidth.readCounter);
    expectedWriteCounters = numberMcChannels * mockIdiWriteVal * transactionSize;
    EXPECT_EQ(expectedWriteCounters, bandwidth.writeCounter);
    expectedBandwidth = mockMaxBwDg2 * mbpsToBytesPerSecond;
    EXPECT_EQ(expectedBandwidth, bandwidth.maxBandwidth);
    EXPECT_GT(bandwidth.timestamp, 0u);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthTwiceThenSuccessIsReturned, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadIdiFilesSuccess);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_bandwidth_t bandwidth;
    uint32_t subDeviceId = 0;
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    expectedReadCounters = numberMcChannels * (mockIdiReadVal + mockDisplayVc1ReadVal) * transactionSize;
    expectedWriteCounters = numberMcChannels * mockIdiWriteVal * transactionSize;
    expectedBandwidth = mockMaxBwDg2 * mbpsToBytesPerSecond;
    EXPECT_EQ(expectedReadCounters, bandwidth.readCounter);
    EXPECT_EQ(expectedWriteCounters, bandwidth.writeCounter);
    EXPECT_EQ(expectedBandwidth, bandwidth.maxBandwidth);
    EXPECT_GT(bandwidth.timestamp, 0u);

    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(expectedReadCounters, bandwidth.readCounter);
    EXPECT_EQ(expectedWriteCounters, bandwidth.writeCounter);
    EXPECT_EQ(expectedBandwidth, bandwidth.maxBandwidth);
    EXPECT_GT(bandwidth.timestamp, 0u);
}

TEST(SysmanProductHelperTest, GivenInvalidProductFamilyWhenCallingProductHelperCreateThenNullPtrIsReturned) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(IGFX_UNKNOWN);
    EXPECT_EQ(nullptr, pSysmanProductHelper);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryPropertiesAndQuerySystemInfoFailsThenDdrMemoryTypeAndNumberOfChannelsAsUnknownAreReturned, IsPVC) {

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->mockQuerySystemInfoReturnValue.push_back(false);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0u;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_EQ(properties.numChannels, -1);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryPropertiesAndQuerySystemInfoSucceedsButMemSysInfoIsNullThenDdrMemoryTypeAndNumberOfChannelsAsUnknownAreReturned, IsPVC) {

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->mockQuerySystemInfoReturnValue.push_back(true);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0u;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_EQ(properties.numChannels, -1);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWithHbm2MemoryTypeWhenCallingGetMemoryPropertiesThenSuccessIsReturned, IsPVC) {

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0u;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWithLpddr4MemoryTypeWhenCallingGetMemoryPropertiesThenSuccessIsReturned, IsPVC) {

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::lpddr4);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0u;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_LPDDR4);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWithLpddr5MemoryTypeWhenCallingGetMemoryPropertiesThenSuccessIsReturned, IsPVC) {

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::lpddr5);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0u;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_LPDDR5);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWithInvalidMemoryTypeWhenCallingGetMemoryPropertiesThenErrorIsReturned, IsPVC) {

    std::unique_ptr<MockMemoryNeoDrm> pDrm = std::make_unique<MockMemoryNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setMemoryType(INT_MAX);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties = {};
    bool isSubdevice = false;
    uint32_t subDeviceId = 0u;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm.get(), pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingMemoryGetStateThenSuccessIsReturned, IsPVC) {

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto pLinuxMemoryImp = std::make_unique<PublicLinuxMemoryImp>(pOsSysman, true, 0);
    zes_mem_state_t state;
    ze_result_t result = pLinuxMemoryImp->getState(&state);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state.size, NEO::probedSizeRegionOne);
    EXPECT_EQ(state.free, NEO::unallocatedSizeRegionOne);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceAndIdiReadFailsWhenCallingZesMemoryGetBandwidthThenErrorIsReturned, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0x4f9502";
        } else if (fd == 6) {
            memcpy(buf, &val, sizeof(val));
            return sizeof(val);
        } else if (fd == 7) {
            oStream << mockMaxBwDg2;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceAndReadingMaxBwFailsWhenCallingGetMemoryBandwidthThenErrorIsReturnedAndMaxBwIsZero, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint64_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0x4f9502";
        } else if (fd == 6) {
            if (offset >= minIdiReadOffset && offset < minIdiWriteOffset) {
                val = mockIdiReadVal;
            } else if (offset >= minIdiWriteOffset && offset < minDisplayVc1ReadOffset) {
                val = mockIdiWriteVal;
            } else if (offset >= minDisplayVc1ReadOffset) {
                val = mockDisplayVc1ReadVal;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            errno = ENOENT;
            return -1;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(bandwidth.maxBandwidth, 0u);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceAndBothVfid0AndVfid1AreTrueWhenGettingMemoryBandwidthThenErrorIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            oStream << "5";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    zes_mem_bandwidth_t bandwidth{};
    ze_result_t result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp, subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanDeviceFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesThenSuccessIsReturned, IsPVC) {

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    MockMemoryFsAccessInterface *pFsAccess = new MockMemoryFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getSysFsAccess();
    pLinuxSysmanImp->pFsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getFsAccess();
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    for (const auto &handle : handles) {
        zes_mem_properties_t properties;
        ze_result_t result = zesMemoryGetProperties(handle, &properties);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, numMemoryChannels);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

HWTEST2_F(SysmanDeviceFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesThenSuccessIsReturned, IsDG1) {

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    for (const auto &handle : handles) {
        zes_mem_properties_t properties;
        ze_result_t result = zesMemoryGetProperties(handle, &properties);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_EQ(properties.busWidth, -1);
    }
}

HWTEST2_F(SysmanDeviceFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesThenSuccessIsReturned, IsDG2) {

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::gddr6);

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    for (const auto &handle : handles) {
        zes_mem_properties_t properties;
        ze_result_t result = zesMemoryGetProperties(handle, &properties);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_GDDR6);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_EQ(properties.busWidth, -1);
    }
}

HWTEST2_F(SysmanDeviceFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVfid1IsActiveThenValidBandwidthAndSuccessIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess64BitRead);

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    for (const auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedBandwidth = 0;

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        expectedReadCounters |= vFHbmHRead;
        expectedReadCounters = (expectedReadCounters << 32) | vFHbmLRead;
        expectedReadCounters = expectedReadCounters * transactionSize;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters |= vFHbmHWrite;
        expectedWriteCounters = (expectedWriteCounters << 32) | vFHbmLWrite;
        expectedWriteCounters = expectedWriteCounters * transactionSize;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthThenSuccessIsReturnedAndBandwidthIsValid, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadIdiFilesSuccess);

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    for (const auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
        ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        expectedReadCounters = numberMcChannels * (mockIdiReadVal + mockDisplayVc1ReadVal) * transactionSize;
        EXPECT_EQ(expectedReadCounters, bandwidth.readCounter);
        expectedWriteCounters = numberMcChannels * mockIdiWriteVal * transactionSize;
        EXPECT_EQ(expectedWriteCounters, bandwidth.writeCounter);
        expectedBandwidth = mockMaxBwDg2 * mbpsToBytesPerSecond;
        EXPECT_EQ(expectedBandwidth, bandwidth.maxBandwidth);
        EXPECT_GT(bandwidth.timestamp, 0u);
    }
}

HWTEST2_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingMemoryPropertiesThenValidMemoryPropertiesRetrieved, IsPVC) {

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        EXPECT_EQ(zesMemoryGetProperties(handle, &properties), ZE_RESULT_SUCCESS);
        EXPECT_TRUE(properties.onSubdevice);
        EXPECT_EQ(properties.physicalSize, strtoull(mockPhysicalSize.c_str(), nullptr, 16));
    }
}

HWTEST2_F(SysmanMultiDeviceFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsOK, IsPVC) {

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    zes_mem_state_t state1;
    ze_result_t result = zesMemoryGetState(handles[0], &state1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state1.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state1.size, NEO::probedSizeRegionOne);
    EXPECT_EQ(state1.free, NEO::unallocatedSizeRegionOne);

    zes_mem_state_t state2;
    result = zesMemoryGetState(handles[1], &state2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state2.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state2.size, NEO::probedSizeRegionFour);
    EXPECT_EQ(state2.free, NEO::unallocatedSizeRegionFour);
}

HWTEST2_F(SysmanMultiDeviceFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsUnknown, IsNotPVC) {

    setupMockDrmWithMemoryType(pSysmanDeviceImp, NEO::DeviceBlobConstants::MemoryType::hbm2e);

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    MockMemoryFsAccessInterface *pFsAccess = new MockMemoryFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getSysFsAccess();
    pLinuxSysmanImp->pFsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getFsAccess();
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    auto handles = getMemoryHandles(pSysmanDevice->toHandle());

    zes_mem_state_t state1;
    ze_result_t result = zesMemoryGetState(handles[0], &state1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state1.health, ZES_MEM_HEALTH_UNKNOWN);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(state1.size, mockIntegratedDeviceAvailableMemory);
        EXPECT_EQ(state1.free, mockIntegratedDeviceFreeMemory);
    } else {
        EXPECT_EQ(state1.size, NEO::probedSizeRegionOne);
        EXPECT_EQ(state1.free, NEO::unallocatedSizeRegionOne);
    }

    zes_mem_state_t state2;
    result = zesMemoryGetState(handles[1], &state2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state2.health, ZES_MEM_HEALTH_UNKNOWN);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(state2.size, mockIntegratedDeviceAvailableMemory);
        EXPECT_EQ(state2.free, mockIntegratedDeviceFreeMemory);
    } else {
        EXPECT_EQ(state2.size, NEO::probedSizeRegionFour);
        EXPECT_EQ(state2.free, NEO::unallocatedSizeRegionFour);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
