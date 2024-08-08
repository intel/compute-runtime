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

const std::map<std::string, uint64_t> dummyKeyOffsetMap = {
    {"DUMMY_KEY", 0x0}};

class ZesPmtFixtureMultiDevice : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockPmtFsAccess> pTestFsAccess;
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        pTestFsAccess = std::make_unique<MockPmtFsAccess>();
        pLinuxSysmanImp->pFsAccess = pTestFsAccess.get();
        L0::Sysman::PlatformMonitoringTech::create(pLinuxSysmanImp, gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject);
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
        for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
};

static int mockReadLinkMultiTelemetryNodesSuccess(const char *path, char *buf, size_t bufsize) {

    std::map<std::string, std::string> fileNameLinkMap = {
        {"/sys/class/intel_pmt/telem1", "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1"},
        {"/sys/class/intel_pmt/telem2", "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem2"},
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
        {"/sys/class/intel_pmt/telem1", "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1"},
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
    if (strPathName == "/sys/class/intel_pmt/telem1/offset" || strPathName == "/sys/class/intel_pmt/telem2/offset") {
        returnValue = 4;
    } else if (strPathName == "/sys/class/intel_pmt/telem1/guid" || strPathName == "/sys/class/intel_pmt/telem2/guid") {
        returnValue = 5;
    } else if (strPathName == "/sys/class/intel_pmt/telem1/telem" || strPathName == "/sys/class/intel_pmt/telem2/telem") {
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

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenenumerateRootTelemIndexThenCheckForErrorIflistDirectoryFails) {
    pTestFsAccess->listDirectoryResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenenumerateRootTelemIndexThenCheckForErrorIfgetRealPathFails) {
    pTestFsAccess->getRealPathResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenWhenenumerateRootTelemIndexThenCheckForErrorIfgetRealPathSuccessButNoTelemetryNodeAndGPUDeviceShareRootPciPort) {
    pTestFsAccess->returnInvalidRealPath = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenTelemDirectoryContainNowTelemEntryWhenenumerateRootTelemIndexThenCheckForError) {
    pTestFsAccess->returnTelemNodes = false;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringWhileValidatingTelemNode) {
    pTestFsAccess->getRealPathResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 0, 0);
    EXPECT_EQ(pPmt->init(pLinuxSysmanImp, gpuUpstreamPortPathInPmt), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringWhileCheckingTelemetryDeviceEntryAvailability) {
    pTestFsAccess->readFileAvailability = ZE_RESULT_ERROR_NOT_AVAILABLE;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 0, 0);
    EXPECT_EQ(pPmt->init(pLinuxSysmanImp, gpuUpstreamPortPathInPmt), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringGUIDRead) {
    pTestFsAccess->readStringResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pLinuxSysmanImp, gpuUpstreamPortPathInPmt), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorIfGUIDReadValueIsNotSupported) {
    pTestFsAccess->readInvalidString = true;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pLinuxSysmanImp, gpuUpstreamPortPathInPmt), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenSomeKeyWhenCallingreadValueWithUint64TypeThenCheckForErrorBranches) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 0, 0);
    uint64_t val = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPmt->readValue("SOMETHING", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenSomeKeyWhenCallingreadValueWithUint32TypeThenCheckForErrorBranches) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 0, 0);
    uint32_t val = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPmt->readValue("SOMETHING", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringbaseOffsetRead) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    pTestFsAccess->readUnsignedResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pLinuxSysmanImp, gpuUpstreamPortPathInPmt), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

inline static int openMockReturnFailure(const char *pathname, int flags) {
    return -1;
}

inline static int openMockReturnSuccess(const char *pathname, int flags) {
    NEO::SysCalls::closeFuncCalled = 0;
    return 0;
}

ssize_t preadMockPmt(int fd, void *buf, size_t count, off_t offset) {
    EXPECT_EQ(0u, NEO::SysCalls::closeFuncCalled);
    *reinterpret_cast<uint32_t *>(buf) = 3u;
    return count;
}

ssize_t preadMockPmt64(int fd, void *buf, size_t count, off_t offset) {
    EXPECT_EQ(0u, NEO::SysCalls::closeFuncCalled);
    *reinterpret_cast<uint64_t *>(buf) = 5u;
    return count;
}

ssize_t preadMockPmtFailure(int fd, void *buf, size_t count, off_t offset) {
    return -1;
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint32TypeAndOpenSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnFailure);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> preadBackup(&NEO::SysCalls::sysCallsPread, preadMockPmt);

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingReadValueWithUint32TypeThenSuccessIsReturned) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> preadBackup(&NEO::SysCalls::sysCallsPread, preadMockPmt);

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPmt->readValue("DUMMY_KEY", val));
    EXPECT_EQ(val, 3u);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingReadValueWithUint64TypeThenSuccessIsReturned) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> preadBackup(&NEO::SysCalls::sysCallsPread, preadMockPmt64);

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPmt->readValue("DUMMY_KEY", val));
    EXPECT_EQ(val, 5u);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint64TypeAndOpenSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnFailure);

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint32TypeAndPreadSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->telemetryDeviceEntry = baseTelemSysFS + "/" + telemNodeForSubdevice0 + "/" + telem;
    pPmt->preadFunction = preadMockPmtFailure;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint64TypeAndPreadSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->telemetryDeviceEntry = baseTelemSysFS + "/" + telemNodeForSubdevice0 + "/" + telem;
    pPmt->preadFunction = preadMockPmtFailure;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenDoingPMTInitThenPMTmapOfSubDeviceIdToPmtObjectWouldContainValidEntries) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    uint32_t subdeviceId = 0;
    do {
        ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), onSubdevice, subdeviceId);
        PublicPlatformMonitoringTech::rootDeviceTelemNodeIndex = 1;
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pLinuxSysmanImp, subdeviceId, pPmt, gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject);
        auto subDeviceIdToPmtEntry = mapOfSubDeviceIdToPmtObject.find(subdeviceId);
        EXPECT_EQ(subDeviceIdToPmtEntry->second, pPmt);
        delete pPmt;

    } while (++subdeviceId < subDeviceCount);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenBaseOffsetReadFailWhenDoingPMTInitThenPMTmapOfSubDeviceIdToPmtObjectWouldBeEmpty) {
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    pTestFsAccess->readUnsignedResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    uint32_t subdeviceId = 0;
    do {
        ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), onSubdevice, subdeviceId);
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pLinuxSysmanImp, subdeviceId, pPmt, gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject);
        EXPECT_TRUE(mapOfSubDeviceIdToPmtObject.empty());

    } while (++subdeviceId < subDeviceCount);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenNoPMTHandleInmapOfSubDeviceIdToPmtObjectWhenCallingreleasePmtObjectThenMapWouldGetEmpty) {
    auto mapOriginal = pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject;
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();

    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    uint32_t subdeviceId = 0;
    do {
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(subdeviceId, nullptr);
    } while (subdeviceId++ < subDeviceCount);

    pLinuxSysmanImp->releasePmtObject();
    EXPECT_TRUE(pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.empty());
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject = mapOriginal;
}

class ZesPmtFixtureNoSubDevice : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockPmtFsAccess> pTestFsAccess;
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pTestFsAccess = std::make_unique<MockPmtFsAccess>();
        pLinuxSysmanImp->pFsAccess = pTestFsAccess.get();
        L0::Sysman::PlatformMonitoringTech::create(pLinuxSysmanImp, gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject);
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
};

TEST_F(ZesPmtFixtureNoSubDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringWhileCheckingTelemetryDeviceEntryAvailability) {
    pTestFsAccess->readFileAvailability = ZE_RESULT_ERROR_NOT_AVAILABLE;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 0, 0);
    EXPECT_EQ(pPmt->init(pLinuxSysmanImp, gpuUpstreamPortPathInPmt), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

HWTEST2_F(ZesPmtFixtureNoSubDevice, GivenTelemNodesAreNotAvailableWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixtureNoSubDevice, GivenOffsetReadFailsWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadOffsetFail);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixtureNoSubDevice, GivenOffsetReadFailsWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadOffsetFail);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixtureNoSubDevice, GivenGuidReadFailsWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadGuidFail);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixtureNoSubDevice, GivenGuidDoesNotGiveKeyOffsetMapWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadIncorrectGuid);

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

HWTEST2_F(ZesPmtFixtureNoSubDevice, GivenTelemDoesNotExistWhenCallingIsTelemetrySupportAvailableThenFalseValueIsReturned, IsPVC) {
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
    pTestFsAccess->readFileAvailability = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    uint32_t subDeviceId = 0;
    EXPECT_FALSE(PlatformMonitoringTech::isTelemetrySupportAvailable(pLinuxSysmanImp, subDeviceId));
}

TEST_F(ZesPmtFixtureNoSubDevice, GivenKeyDoesNotExistinKeyOffsetMapWhenCallingReadValueForUnsignedIntThenFalseValueIsReturned) {
    uint32_t value = 0;
    std::string mockTelemDir = sysfsPathTelem1;
    uint64_t mockOffset = 0;
    std::map<std::string, uint64_t> keyOffsetMap = {{"PACKAGE_ENERGY", 1032}, {"SOC_TEMPERATURES", 56}};
    std::string mockKey = "ABCDE";
    EXPECT_FALSE(PlatformMonitoringTech::readValue(keyOffsetMap, mockTelemDir, mockKey, mockOffset, value));
}

TEST_F(ZesPmtFixtureNoSubDevice, GivenKeyDoesNotExistinKeyOffsetMapWhenCallingReadValueForUnsignedLongThenFalseValueIsReturned) {
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
