/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_pmt.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

const std::map<std::string, uint64_t> dummyKeyOffsetMap = {
    {"DUMMY_KEY", 0x0}};

class ZesPmtFixtureMultiDevice : public SysmanMultiDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<MockPmtFsAccess> pTestFsAccess;
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
        pTestFsAccess = std::make_unique<MockPmtFsAccess>();
        PlatformMonitoringTech::create(deviceHandles, pTestFsAccess.get(), gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject);
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
    pTestFsAccess->listDirectoryResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenenumerateRootTelemIndexThenCheckForErrorIfgetRealPathFails) {
    pTestFsAccess->getRealPathResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenWhenenumerateRootTelemIndexThenCheckForErrorIfgetRealPathSuccessButNoTelemetryNodeAndGPUDeviceShareRootPciPort) {
    pTestFsAccess->returnInvalidRealPath = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenTelemDirectoryContainNowTelemEntryWhenenumerateRootTelemIndexThenCheckForError) {
    pTestFsAccess->returnTelemNodes = false;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt));
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringWhileValidatingTelemNode) {
    pTestFsAccess->getRealPathResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), gpuUpstreamPortPathInPmt, productFamily), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorThatCouldHappenDuringGUIDRead) {
    pTestFsAccess->readStringResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), gpuUpstreamPortPathInPmt, productFamily), ZE_RESULT_ERROR_NOT_AVAILABLE);
}

TEST_F(ZesPmtFixtureMultiDevice, GivenValidDeviceHandlesWhenCreatingPMTHandlesThenCheckForErrorIfGUIDReadValueIsNotSupported) {
    pTestFsAccess->readInvalidString = true;
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
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
    PlatformMonitoringTech::enumerateRootTelemIndex(pTestFsAccess.get(), gpuUpstreamPortPathInPmt);
    auto productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    auto pPmt = std::make_unique<PublicPlatformMonitoringTech>(pTestFsAccess.get(), 1, 0);
    EXPECT_EQ(pPmt->init(pTestFsAccess.get(), gpuUpstreamPortPathInPmt, productFamily), ZE_RESULT_ERROR_NOT_AVAILABLE);
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
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    for (const auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
        auto productFamily = SysmanDeviceImp::getProductFamily(Device::fromHandle(deviceHandle));
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), deviceProperties.subdeviceId, pPmt,
                                                      gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject, productFamily);
        auto subDeviceIdToPmtEntry = mapOfSubDeviceIdToPmtObject.find(deviceProperties.subdeviceId);
        EXPECT_EQ(subDeviceIdToPmtEntry->second, pPmt);
        delete pPmt;
    }
}

TEST_F(ZesPmtFixtureMultiDevice, GivenBaseOffsetReadFailWhenDoingPMTInitThenPMTmapOfSubDeviceIdToPmtObjectWouldBeEmpty) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    pTestFsAccess->readUnsignedResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new PublicPlatformMonitoringTech(pTestFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
        auto productFamily = SysmanDeviceImp::getProductFamily(Device::fromHandle(deviceHandle));
        UNRECOVERABLE_IF(nullptr == pPmt);
        PublicPlatformMonitoringTech::doInitPmtObject(pTestFsAccess.get(), deviceProperties.subdeviceId, pPmt,
                                                      gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject, productFamily);
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
    std::unique_ptr<MockPmtFsAccess> pTestFsAccess;
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
        pTestFsAccess = std::make_unique<MockPmtFsAccess>();
        PlatformMonitoringTech::create(deviceHandles, pTestFsAccess.get(), gpuUpstreamPortPathInPmt, mapOfSubDeviceIdToPmtObject);
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
