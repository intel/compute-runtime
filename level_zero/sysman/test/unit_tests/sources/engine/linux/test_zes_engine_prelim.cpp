/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/engine/linux/mock_engine_prelim.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t handleComponentCount = 13u;
constexpr uint32_t handleCountForMultiDeviceFixture = 7u;
class ZesEngineFixture : public SysmanDeviceFixture {
  protected:
    MockEngineNeoDrm *pDrm = nullptr;
    std::unique_ptr<MockEnginePmuInterfaceImp> pPmuInterface;
    Drm *pOriginalDrm = nullptr;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    std::unique_ptr<MockEngineSysfsAccess> pSysfsAccess;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockEngineFsAccess> pFsAccess;
    L0::Sysman::FsAccessInterface *pFsAccessOriginal = nullptr;

    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEngineSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEngineFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pDrm = new MockEngineNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockEngineNeoDrm>(pDrm));

        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImp>(pLinuxSysmanImp);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
        device = pSysmanDevice;
        getEngineHandles(0);
    }

    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
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

    MockEngineNeoDrm *pDrm = nullptr;
    int mockFd = -1;
    pDrm = new MockEngineNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()), mockFd);
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<MockEngineNeoDrm>(pDrm));

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    L0::Sysman::SysmanDevice *device = pSysmanDevice;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
}

TEST_F(ZesEngineFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
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

TEST_F(ZesEngineFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
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

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(mockActiveTime / microSecondsToNanoSeconds, stats.activeTime);
        EXPECT_EQ(mockTimestamp / microSecondsToNanoSeconds, stats.timestamp);
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndDiscreteDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {

    pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(mockActiveTime / microSecondsToNanoSeconds, stats.activeTime);
        EXPECT_EQ(mockTimestamp / microSecondsToNanoSeconds, stats.timestamp);
    }
}

TEST_F(ZesEngineFixture, GivenTestDiscreteDevicesAndValidEngineHandleWhenCallingZesEngineGetActivityAndPMUGetEventTypeFailsThenVerifyEngineGetActivityReturnsFailure) {

    pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    pSysfsAccess->mockReadSymLinkFailure = true;

    auto pOsEngineTest1 = L0::Sysman::OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);

    zes_engine_stats_t stats = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest1->getActivity(&stats));

    pSysfsAccess->mockReadSymLinkSuccess = true;
    pFsAccess->mockReadVal = true;

    auto pOsEngineTest2 = L0::Sysman::OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest2->getActivity(&stats));
}

TEST_F(ZesEngineFixture, GivenUnknownEngineTypeThengetEngineGroupFromTypeReturnsGroupAllEngineGroup) {
    auto group = L0::Sysman::LinuxEngineImp::getGroupFromEngineType(ZES_ENGINE_GROUP_3D_SINGLE);
    EXPECT_EQ(group, ZES_ENGINE_GROUP_ALL);
}

TEST_F(ZesEngineFixture, GivenTestIntegratedDevicesAndValidEngineHandleWhenCallingZesEngineGetActivityAndPMUGetEventTypeFailsThenVerifyEngineGetActivityReturnsFailure) {
    zes_engine_stats_t stats = {};

    pFsAccess->mockReadVal = true;

    auto pOsEngineTest1 = L0::Sysman::OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest1->getActivity(&stats));
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndPmuReadFailsThenVerifyEngineGetActivityReturnsFailure) {

    pPmuInterface->mockPmuRead = true;

    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivity(handle, &stats));
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityExtThenUnsupportedFeatureErrorIsReturned) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, nullptr, &stats));
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndperfEventOpenFailsThenVerifyEngineGetActivityReturnsFailure) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << 23;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPerfEventOpenRead = true;
    EXPECT_EQ(-1, pPmuInterface->pmuInterfaceOpen(0, -1, 0));
}

TEST_F(ZesEngineFixture, GivenValidOsSysmanPointerWhenRetrievingEngineTypeAndInstancesAndIfEngineInfoQueryFailsThenErrorIsReturned) {
    std::set<std::pair<zes_engine_group_t, std::pair<uint32_t, uint32_t>>> engineGroupInstance;

    pDrm->mockReadSysmanQueryEngineInfo = true;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::Sysman::OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman));
}

TEST_F(ZesEngineFixture, GivenHandleQueryItemCalledWithInvalidEngineTypeThenzesDeviceEnumEngineGroupsSucceeds) {

    uint32_t count = 0;
    uint32_t mockHandleCount = 13u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesEngineFixture, GivenHandleQueryItemCalledWhenPmuInterfaceOpenFailsThenzesDeviceEnumEngineGroupsSucceedsAndHandleCountIsZero) {

    pFsAccess->mockReadVal = true;

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());
    uint32_t count = 0;
    uint32_t mockHandleCount = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesEngineFixture, GivenValidDrmObjectWhenCallingsysmanQueryEngineInfoMethodThenSuccessIsReturned) {
    auto drm = std::make_unique<DrmMockEngine>(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    ASSERT_NE(nullptr, drm);
    EXPECT_TRUE(drm->sysmanQueryEngineInfo());
    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndHandleCountZeroWhenCallingReInitThenValidCountIsReturnedAndVerifyzesDeviceEnumEngineGroupsSucceeds) {

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

class ZesEngineMultiFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockEnginePmuInterfaceImp> pPmuInterface;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    std::unique_ptr<MockEngineSysfsAccess> pSysfsAccess;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockEngineFsAccess> pFsAccess;
    L0::Sysman::FsAccessInterface *pFsAccessOriginal = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEngineSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEngineFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        MockEngineNeoDrm *pDrm = new MockEngineNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockEngineNeoDrm>(pDrm));

        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImp>(pLinuxSysmanImp);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pDrm->mockReadSysmanQueryEngineInfoMultiDevice = true;
        pSysfsAccess->mockReadSymLinkSuccess = true;

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
        device = pSysmanDevice;
        getEngineHandles(0);
    }

    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
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

TEST_F(ZesEngineMultiFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleCountForMultiDeviceFixture);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleCountForMultiDeviceFixture);
}

TEST_F(ZesEngineMultiFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    auto handles = getEngineHandles(handleCountForMultiDeviceFixture);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[4], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[5], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[6], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
}

TEST_F(ZesEngineMultiFixture, GivenHandleQueryItemCalledWhenPmuInterfaceOpenFailsThenzesDeviceEnumEngineGroupsSucceedsAndHandleCountIsZero) {

    pFsAccess->mockReadVal = true;

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());
    uint32_t count = 0;
    uint32_t mockHandleCount = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
