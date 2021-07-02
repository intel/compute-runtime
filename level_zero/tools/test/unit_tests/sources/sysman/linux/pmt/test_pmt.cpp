/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_pmt.h"

using ::testing::_;
using ::testing::Matcher;
using ::testing::Return;

namespace L0 {
namespace ult {
static int fakeFileDescriptor = 123;

class ZesPmtFixtureMultiDevice : public SysmanMultiDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<Mock<PmtFsAccess>> pTestFsAccess;
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    void SetUp() override {
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
        SysmanMultiDeviceFixture::TearDown();
        for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
            delete subDeviceIdToPmtEntry.second;
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

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringSizeRead) {
    EXPECT_CALL(*pTestFsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringbaseOffsetRead) {
    EXPECT_CALL(*pTestFsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillOnce(Return(ZE_RESULT_SUCCESS))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt);
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenSomeKeyWhenCallingreadValueWithUint64TypeThenCheckForErrorBranches) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 0, 0);
    pPmt->mappedMemory = nullptr;
    uint64_t val = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pPmt->readValue("SOMETHING", val));

    uint32_t maxIndex = 10u;
    pPmt->mappedMemory = new char[maxIndex];
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPmt->readValue("SOMETHING", val));
    delete pPmt->mappedMemory;
}

inline static int openMock(const char *pathname, int flags, ...) {
    if (strcmp(pathname, "/sys/class/intel_pmt/telem2/telem") == 0) {
        return fakeFileDescriptor;
    }
    if (strcmp(pathname, "/sys/class/intel_pmt/telem3/telem") == 0) {
        return fakeFileDescriptor;
    }
    return -1;
}

inline static int openMockReturnFailure(const char *pathname, int flags, ...) {
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

inline static void *mmapMockReturnFailure(void *addr, size_t length, int prot, int flags, int fd, off_t offset) noexcept {
    return MAP_FAILED;
}

inline static void *mmapMock(void *addr, size_t length, int prot, int flags, int fd, off_t offset) noexcept {
    void *ptr = nullptr;
    if (fd == fakeFileDescriptor) {
        ptr = alignedMalloc(length, MemoryConstants::pageSize64k);
        return ptr;
    }
    return MAP_FAILED;
}

inline static int munmapMock(void *addr, size_t length) noexcept {
    alignedFree(addr);
    return 0;
}

inline static int munmapMockDoNothing(void *addr, size_t length) noexcept {
    return 0;
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenDoingPMTInitThenPMTInitSucceed) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->openFunction = openMock;
    pPmt->mmapFunction = mmapMock;
    pPmt->munmapFunction = munmapMock;
    pPmt->closeFunction = closeMock;
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_SUCCESS);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenDoingPMTInitAndOpenSysCallFailsThenPMTInitFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->openFunction = openMockReturnFailure;
    pPmt->mmapFunction = mmapMock;
    pPmt->munmapFunction = munmapMock;
    pPmt->closeFunction = closeMock;
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenDoingPMTInitAndmmapSysCallFailsThenPMTInitFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->openFunction = openMock;
    pPmt->mmapFunction = mmapMockReturnFailure;
    pPmt->munmapFunction = munmapMockDoNothing;
    pPmt->closeFunction = closeMock;
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenDoingPMTInitAndcloseSysCallFailsThenPMTInitFails) {
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    pPmt->openFunction = openMock;
    pPmt->mmapFunction = mmapMock;
    pPmt->munmapFunction = munmapMock;
    pPmt->closeFunction = closeMockReturnFailure;
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), rootPciPathOfGpuDeviceInPmt), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidSyscallsWhenDoingPMTInitThenPMTmapOfSubDeviceIdToPmtObjectWouldContainValidEntries) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    for (const auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
        UNRECOVERABLE_IF(nullptr == pPmt);
        pPmt->openFunction = openMock;
        pPmt->mmapFunction = mmapMock;
        pPmt->munmapFunction = munmapMock;
        pPmt->closeFunction = closeMock;
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), deviceProperties.subdeviceId, pPmt,
                                                      rootPciPathOfGpuDeviceInPmt, mapOfSubDeviceIdToPmtObject);
        auto subDeviceIdToPmtEntry = mapOfSubDeviceIdToPmtObject.find(deviceProperties.subdeviceId);
        EXPECT_EQ(subDeviceIdToPmtEntry->second, pPmt);
        delete pPmt;
    }
}

TEST_F(ZesPmtFixtureMultiDevice, GivenOpenSyscallFailWhenDoingPMTInitThenPMTmapOfSubDeviceIdToPmtObjectWouldBeEmpty) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    for (const auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
        UNRECOVERABLE_IF(nullptr == pPmt);
        pPmt->openFunction = openMockReturnFailure;
        pPmt->mmapFunction = mmapMock;
        pPmt->munmapFunction = munmapMock;
        pPmt->closeFunction = closeMock;
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), deviceProperties.subdeviceId, pPmt,
                                                      rootPciPathOfGpuDeviceInPmt, mapOfSubDeviceIdToPmtObject);
        EXPECT_TRUE(mapOfSubDeviceIdToPmtObject.empty());
    }
}

TEST_F(ZesPmtFixtureMultiDevice, GivenNoPMTHandleInmapOfSubDeviceIdToPmtObjectWhenCallingreleasePmtObjectThenMapWouldGetEmpty) {
    auto mapOriginal = pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject;
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
        SysmanDeviceFixture::TearDown();
        for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
            delete subDeviceIdToPmtEntry.second;
        }
    }
};

TEST_F(ZesPmtFixtureNoSubDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenValidPmtHandlesForAllSubdevicesWillBeCreated) {}

} // namespace ult
} // namespace L0