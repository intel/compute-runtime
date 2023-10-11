/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

#include "mock_pmt.h"

namespace L0 {
namespace Sysman {
namespace ult {

static int fakeFileDescriptor = 123;

const std::map<std::string, uint64_t> dummyKeyOffsetMap = {
    {"DUMMY_KEY", 0x0}};

class ZesPmtFixtureMultiDevice : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockPmtFsAccess> pTestFsAccess;
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        pTestFsAccess = std::make_unique<MockPmtFsAccess>();
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

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenValidPmtHandlesForAllSubdevicesWillBeCreated) {}

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
    auto productFamily = pSysmanDeviceImp->getProductFamily();
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), gpuUpstreamPortPathInPmt, productFamily), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringGUIDRead) {
    pTestFsAccess->readStringResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto productFamily = pSysmanDeviceImp->getProductFamily();
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), gpuUpstreamPortPathInPmt, productFamily), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorIfGUIDReadValueIsNotSupported) {
    pTestFsAccess->readInvalidString = true;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto productFamily = pSysmanDeviceImp->getProductFamily();
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), gpuUpstreamPortPathInPmt, productFamily), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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
    pTestFsAccess->readUnsignedResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    L0::Sysman::PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto productFamily = pSysmanDeviceImp->getProductFamily();
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), gpuUpstreamPortPathInPmt, productFamily), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

inline static int openMock(const char *pathname, int flags) {
    if (strcmp(pathname, "/sys/class/intel_pmt/telem2/telem") == 0) {
        return fakeFileDescriptor;
    }
    if (strcmp(pathname, "/sys/class/intel_pmt/telem3/telem") == 0) {
        return fakeFileDescriptor;
    }
    return -1;
}

inline static int openMockReturnFailure(const char *pathname, int flags) {
    return -1;
}

inline static int closeMock(int fd) {
    if (fd == fakeFileDescriptor) {
        return 0;
    }
    return -1;
}

inline static int closeMockReturnFailure(int fd) {
    return -1;
}

ssize_t preadMockPmt(int fd, void *buf, size_t count, off_t offset) {
    return count;
}

ssize_t preadMockPmtFailure(int fd, void *buf, size_t count, off_t offset) {
    return -1;
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint32TypeAndOpenSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->openFunction = openMockReturnFailure;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint32TypeAndCloseSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->telemetryDeviceEntry = baseTelemSysFS + "/" + telemNodeForSubdevice0 + "/" + telem;
    pPmt->openFunction = openMock;
    pPmt->preadFunction = preadMockPmt;
    pPmt->closeFunction = closeMockReturnFailure;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint64TypeAndOpenSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->openFunction = openMockReturnFailure;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint64TypeAndCloseSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->telemetryDeviceEntry = baseTelemSysFS + "/" + telemNodeForSubdevice0 + "/" + telem;
    pPmt->openFunction = openMock;
    pPmt->preadFunction = preadMockPmt;
    pPmt->closeFunction = closeMockReturnFailure;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint32TypeAndPreadSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->telemetryDeviceEntry = baseTelemSysFS + "/" + telemNodeForSubdevice0 + "/" + telem;
    pPmt->openFunction = openMock;
    pPmt->preadFunction = preadMockPmtFailure;
    pPmt->closeFunction = closeMock;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenCallingreadValueWithUint64TypeAndPreadSysCallFailsThenreadValueFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->telemetryDeviceEntry = baseTelemSysFS + "/" + telemNodeForSubdevice0 + "/" + telem;
    pPmt->openFunction = openMock;
    pPmt->preadFunction = preadMockPmtFailure;
    pPmt->closeFunction = closeMock;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenDoingPMTInitThenPMTmapOfSubDeviceIdToPmtObjectWouldContainValidEntries) {
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    uint32_t subdeviceId = 0;
    do {
        ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), onSubdevice, subdeviceId);
        PublicPlatformMonitoringTech::rootDeviceTelemNodeIndex = 1;
        auto productFamily = pSysmanDeviceImp->getProductFamily();
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), subdeviceId, pPmt,
                                                      gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject, productFamily);
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
        auto productFamily = pSysmanDeviceImp->getProductFamily();
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), subdeviceId, pPmt,
                                                      gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject, productFamily);
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

TEST_F(ZesPmtFixtureNoSubDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenValidPmtHandlesForAllSubdevicesWillBeCreated) {}

} // namespace ult
} // namespace Sysman
} // namespace L0
