/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/engine/linux/mock_engine.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t handleComponentCount = 6u;
constexpr uint32_t microSecondsToNanoSeconds = 1000u;

class ZesEngineFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
};

class ZesEngineFixtureI915 : public ZesEngineFixture {
  protected:
    std::unique_ptr<MockEnginePmuInterfaceImp> pPmuInterface;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });
        MockEngineNeoDrm *pDrm = new MockEngineNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockEngineNeoDrm>(pDrm));

        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceI915Upstream(pLinuxSysmanImp->getSysmanProductHelper()));
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pDrm);

        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImp>(pLinuxSysmanImp);
        pPmuInterface->mockPmuFd = 10;
        pPmuInterface->mockActiveTime = 987654321;
        pPmuInterface->mockTimestamp = 87654321;
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
        device = pSysmanDevice;
        getEngineHandles(0);
    }

    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_engine_handle_t> getEngineHandles(uint32_t count) {

        VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
            uint32_t mockReadVal = 23;
            std::ostringstream oStream;
            oStream << mockReadVal;
            std::string value = oStream.str();
            memcpy(buf, value.data(), count);
            return count;
        });

        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesEngineFixtureI915, GivenComponentCountZeroWhenCallingzesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {

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

TEST_F(ZesEngineFixtureI915, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    auto handle = getEngineHandles(handleComponentCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[4], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COPY_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[5], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}

TEST_F(ZesEngineFixtureI915, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(pPmuInterface->mockActiveTime / microSecondsToNanoSeconds, stats.activeTime);
        EXPECT_EQ(pPmuInterface->mockTimestamp / microSecondsToNanoSeconds, stats.timestamp);
    }
}

TEST_F(ZesEngineFixtureI915, GivenValidEngineHandleAndDiscreteDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(pPmuInterface->mockActiveTime / microSecondsToNanoSeconds, stats.activeTime);
        EXPECT_EQ(pPmuInterface->mockTimestamp / microSecondsToNanoSeconds, stats.timestamp);
    }
}

TEST_F(ZesEngineFixtureI915, GivenTestDiscreteDevicesAndValidEngineHandleWhenCallingZesEngineGetActivityAndPMUGetEventTypeFailsThenVerifyEngineGetActivityReturnsFailure) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });

    pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    auto pOsEngineTest1 = L0::Sysman::OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);

    zes_engine_stats_t stats = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest1->getActivity(&stats));

    auto pOsEngineTest2 = L0::Sysman::OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest2->getActivity(&stats));
}

TEST_F(ZesEngineFixtureI915, GivenTestIntegratedDevicesAndValidEngineHandleWhenCallingZesEngineGetActivityAndPMUGetEventTypeFailsThenVerifyEngineGetActivityReturnsFailure) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return -1;
    });

    zes_engine_stats_t stats = {};
    auto pOsEngineTest1 = L0::Sysman::OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest1->getActivity(&stats));
}

TEST_F(ZesEngineFixtureI915, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndPmuReadFailsThenVerifyEngineGetActivityReturnsFailure) {
    pPmuInterface->mockPmuReadFailureReturnValue = -1;
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivity(handle, &stats));
    }
}

TEST_F(ZesEngineFixtureI915, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndperfEventOpenFailsThenVerifyEngineGetActivityReturnsFailure) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint32_t mockReadVal = 23;
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPerfEventFailureReturnValue = -1;
    EXPECT_EQ(-1, pPmuInterface->pmuInterfaceOpen(0, -1, 0));
}

TEST_F(ZesEngineFixtureI915, GivenValidOsSysmanPointerWhenRetrievingEngineTypeAndInstancesAndIfEngineInfoQueryFailsThenErrorIsReturned) {
    std::set<std::pair<zes_engine_group_t, L0::Sysman::EngineInstanceSubDeviceId>> engineGroupInstance;

    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    auto *pDrm = osInterface->getDriverModel()->as<MockEngineNeoDrm>();
    pDrm->mockSysmanQueryEngineInfoReturnFalse = false;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, L0::Sysman::OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman));
}

TEST_F(ZesEngineFixtureI915, givenEngineInfoQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsCreatedWithEngines) {
    auto drm = std::make_unique<DrmMockEngine>((const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment())));
    ASSERT_NE(nullptr, drm);
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->sysmanQueryEngineInfo();
    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->getEngineInfos().size());
}

TEST_F(ZesEngineFixtureI915, GivenEngineInfoWithVideoQuerySupportedWhenQueryingEngineInfoWithVideoThenEngineInfoIsCreatedWithEngines) {
    auto drm = std::make_unique<DrmMockEngine>((const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment())));
    ASSERT_NE(nullptr, drm);
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->sysmanQueryEngineInfo();
    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->getEngineInfos().size());
}

TEST_F(ZesEngineFixtureI915, GivenEngineInfoWithVideoQueryFailsThenFailureIsReturned) {
    auto drm = std::make_unique<DrmMockEngineInfoFailing>((const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment())));
    ASSERT_NE(nullptr, drm);
    EXPECT_FALSE(drm->sysmanQueryEngineInfo());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
