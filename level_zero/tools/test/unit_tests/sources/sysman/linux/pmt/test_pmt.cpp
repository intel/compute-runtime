/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_pmt.h"

extern bool sysmanUltsEnable;

using ::testing::_;
using ::testing::Matcher;
using ::testing::Return;

namespace L0 {
namespace ult {
static int fakeFileDescriptor = 123;

const std::map<std::string, uint64_t> dummyKeyOffsetMap = {
    {"DUMMY_KEY", 0x0}};

class ZesPmtFixtureMultiDevice : public SysmanMultiDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<Mock<PmtFsAccess>> pTestFsAccess;
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pTestFsAccess = std::make_unique<NiceMock<Mock<PmtFsAccess>>>();
        ON_CALL(*pTestFsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::getValString));
        ON_CALL(*pTestFsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::getValUnsignedLong));
        ON_CALL(*pTestFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::listDirectorySuccess));
        ON_CALL(*pTestFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::getRealPathSuccess));
        ON_CALL(*pTestFsAccess.get(), fileExists(_))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::isFileExists));
        PlatformMonitoringTech::create(deviceHandles, pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt, mapOfSubDeviceIdToPmtObject);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::TearDown();
        for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
};

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenValidPmtHandlesForAllSubdevicesWillBeCreated) {}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenenumerateRootTelemIndexThenCheckForErrorIflistDirectoryFails) {
    EXPECT_CALL(*pTestFsAccess.get(), listDirectory(_, _))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenenumerateRootTelemIndexThenCheckForErrorIfgetRealPathFails) {
    EXPECT_CALL(*pTestFsAccess.get(), getRealPath(_, _))
        .WillRepeatedly(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenWhenenumerateRootTelemIndexThenCheckForErrorIfgetRealPathSuccessButNoTelemetryNodeAndGPUDeviceShareRootPciPort) {
    EXPECT_CALL(*pTestFsAccess.get(), getRealPath(_, _))
        .Times(5)
        .WillRepeatedly(::testing::DoAll(::testing::SetArgReferee<1>("/sys/devices/pci0000:89/0000:89:02.0/0000:8e:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1"), Return(ZE_RESULT_SUCCESS)));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenTelemDirectoryContainNowTelemEntryWhenenumerateRootTelemIndexThenCheckForError) {
    ON_CALL(*pTestFsAccess.get(), listDirectory(_, _))
        .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::listDirectoryNoTelemNode));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringWhileValidatingTelemNode) {
    EXPECT_CALL(*pTestFsAccess.get(), getRealPath(_, _))
        .WillRepeatedly(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringGUIDRead) {
    EXPECT_CALL(*pTestFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorIfGUIDReadValueIsNotSupported) {
    EXPECT_CALL(*pTestFsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<1>(""), Return(ZE_RESULT_SUCCESS)));
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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
    EXPECT_CALL(*pTestFsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_NOT_AVAILABLE);
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
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    for (const auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), deviceProperties.subdeviceId, pPmt,
                                                      rootPciPathOfGpuDeviceInPmt, mapOfSubDeviceIdToPmtObject);
        auto subDeviceIdToPmtEntry = mapOfSubDeviceIdToPmtObject.find(deviceProperties.subdeviceId);
        EXPECT_EQ(subDeviceIdToPmtEntry->second, pPmt);
        delete pPmt;
    }
}

TEST_F(ZesPmtFixtureMultiDevice, GivenBaseOffsetReadFailWhenDoingPMTInitThenPMTmapOfSubDeviceIdToPmtObjectWouldBeEmpty) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    EXPECT_CALL(*pTestFsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillRepeatedly(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    for (const auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), deviceProperties.subdeviceId, pPmt,
                                                      rootPciPathOfGpuDeviceInPmt, mapOfSubDeviceIdToPmtObject);
        EXPECT_TRUE(mapOfSubDeviceIdToPmtObject.empty());
    }
}

TEST_F(ZesPmtFixtureMultiDevice, GivenNoPMTHandleInmapOfSubDeviceIdToPmtObjectWhenCallingreleasePmtObjectThenMapWouldGetEmpty) {
    auto mapOriginal = pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject;
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    for (const auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, nullptr);
    }
    pLinuxSysmanImp->releasePmtObject();
    EXPECT_TRUE(pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.empty());
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject = mapOriginal;
}

class ZesPmtFixtureNoSubDevice : public SysmanDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<Mock<PmtFsAccess>> pTestFsAccess;
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pTestFsAccess = std::make_unique<NiceMock<Mock<PmtFsAccess>>>();
        ON_CALL(*pTestFsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::getValString));
        ON_CALL(*pTestFsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::getValUnsignedLong));
        ON_CALL(*pTestFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::listDirectorySuccess));
        ON_CALL(*pTestFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::getRealPathSuccess));
        ON_CALL(*pTestFsAccess.get(), fileExists(_))
            .WillByDefault(::testing::Invoke(pTestFsAccess.get(), &Mock<PmtFsAccess>::isFileExists));
        PlatformMonitoringTech::create(deviceHandles, pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt, mapOfSubDeviceIdToPmtObject);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
};

TEST_F(ZesPmtFixtureNoSubDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenValidPmtHandlesForAllSubdevicesWillBeCreated) {}

} // namespace ult
} // namespace L0
