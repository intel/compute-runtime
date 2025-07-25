/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/engine/linux/mock_engine_prelim.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t handleComponentCount = 13u;

class ZesEngineFixturePrelim : public SysmanDeviceFixture {
  protected:
    MockEngineNeoDrmPrelim *pDrm = nullptr;
    std::unique_ptr<MockEnginePmuInterfaceImpPrelim> pPmuInterface;
    Drm *pOriginalDrm = nullptr;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;

    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });

        pDrm = new MockEngineNeoDrmPrelim(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockEngineNeoDrmPrelim>(pDrm));

        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceI915Prelim(pLinuxSysmanImp->getSysmanProductHelper()));
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pDrm);

        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImpPrelim>(pLinuxSysmanImp);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        bool isIntegratedDevice = true;
        pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
        device = pSysmanDevice;
        getEngineHandles(0);
    }

    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_engine_handle_t> getEngineHandles(uint32_t count) {

        VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
            std::ostringstream oStream;
            oStream << 23;
            std::string value = oStream.str();
            memcpy(buf, value.data(), count);
            return count;
        });

        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFixture, GivenComponentCountZeroAndOpenCallFailsWhenCallingZesDeviceEnumEngineGroupsThenErrorIsReturned) {

    MockEngineNeoDrmPrelim *pDrm = nullptr;
    int mockFd = -1;
    pDrm = new MockEngineNeoDrmPrelim(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()), mockFd);
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<MockEngineNeoDrmPrelim>(pDrm));

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    L0::Sysman::SysmanDevice *device = pSysmanDevice;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
}

TEST_F(ZesEngineFixturePrelim, GivenComponentCountZeroWhenCallingZesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCount);
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    auto handles = getEngineHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COMPUTE_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COPY_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[4], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COMPUTE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[5], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[6], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[7], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[8], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[9], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[10], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COPY_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[11], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[12], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(stats.activeTime, mockActiveTime);
        EXPECT_EQ(stats.timestamp, pPmuInterface->mockTimestamp);
    }
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandleAndPmuTimeStampIsZeroWhenCallingZesEngineGetActivityThenValidTimeStampIsReturned) {
    zes_engine_stats_t stats = {};
    pPmuInterface->mockTimestamp = 0u;
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    uint64_t timeBeforeApiCall = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(stats.activeTime, mockActiveTime);
        EXPECT_GE(stats.timestamp, timeBeforeApiCall);
    }
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandleAndDiscreteDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {

    bool isIntegratedDevice = false;
    pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(stats.activeTime, mockActiveTime);
        EXPECT_EQ(stats.timestamp, pPmuInterface->mockTimestamp);
    }
}

TEST_F(ZesEngineFixturePrelim, GivenUnknownEngineTypeThengetEngineGroupFromTypeReturnsGroupAllEngineGroup) {
    auto group = L0::Sysman::LinuxEngineImp::getGroupFromEngineType(ZES_ENGINE_GROUP_3D_SINGLE);
    EXPECT_EQ(group, ZES_ENGINE_GROUP_ALL);
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndPmuReadFailsThenVerifyEngineGetActivityReturnsFailure) {

    pPmuInterface->mockPmuRead = true;

    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesEngineGetActivity(handle, &stats));
    }
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityExtThenUnsupportedFeatureErrorIsReturned) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, nullptr, &stats));
    }
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndperfEventOpenFailsThenVerifyEngineGetActivityReturnsFailure) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 23;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPerfEventOpenReadFail = true;
    EXPECT_EQ(-1, pPmuInterface->pmuInterfaceOpen(0, -1, 0));
}

TEST_F(ZesEngineFixturePrelim, GivenValidOsSysmanPointerWhenRetrievingEngineTypeAndInstancesAndIfEngineInfoQueryFailsThenErrorIsReturned) {
    std::set<std::pair<zes_engine_group_t, std::pair<uint32_t, uint32_t>>> engineGroupInstance;

    pDrm->mockReadSysmanQueryEngineInfo = true;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::Sysman::OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman));
}

TEST_F(ZesEngineFixturePrelim, GivenHandleQueryItemCalledWithInvalidEngineTypeThenzesDeviceEnumEngineGroupsSucceeds) {

    uint32_t count = 0;
    uint32_t mockHandleCount = 13u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesEngineFixturePrelim, GivenDeviceHandleAndPmuOpenFailsDueToTooManyOpenFilesWhenCallingZesDeviceEnumEngineGroupsThenZeroHandlesReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 23;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPerfEventOpenReadFail = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockErrorNumber = EMFILE;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());

    uint32_t handleCount = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &handleCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(handleCount, 0u);
}

TEST_F(ZesEngineFixturePrelim, GivenDeviceHandleAndPmuOpenFailsDueToFileTableOverFlowWhenCallingZesDeviceEnumEngineGroupsThenZeroHandlesReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 23;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPerfEventOpenReadFail = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockErrorNumber = ENFILE;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());

    uint32_t handleCount = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &handleCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(handleCount, 0u);
}

TEST_F(ZesEngineFixturePrelim, GivenPmuOpenFailsWhenCallingZesDeviceEnumEngineGroupsThenZeroHandlesAreEnumerated) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 23;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPerfEventOpenReadFail = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());

    uint32_t handleCount = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &handleCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(handleCount, 0u);
}

TEST_F(ZesEngineFixturePrelim, GivenHandleQueryItemCalledWhenPmuInterfaceOpenFailsThenzesDeviceEnumEngineGroupsSucceedsAndHandleCountIsZero) {

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());
    uint32_t count = 0;
    uint32_t mockHandleCount = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesEngineFixturePrelim, GivenValidDrmObjectWhenCallingsysmanQueryEngineInfoMethodThenSuccessIsReturned) {
    auto drm = std::make_unique<DrmMockEngine>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    ASSERT_NE(nullptr, drm);
    EXPECT_TRUE(drm->sysmanQueryEngineInfo());
    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
}

TEST_F(ZesEngineFixturePrelim, GivenValidEngineHandleAndHandleCountZeroWhenCallingReInitThenValidCountIsReturnedAndVerifyzesDeviceEnumEngineGroupsSucceeds) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 23;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
