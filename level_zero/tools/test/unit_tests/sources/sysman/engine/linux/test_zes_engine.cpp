/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_engine.h"

extern bool sysmanUltsEnable;

class OsEngine;
namespace L0 {
namespace ult {
constexpr uint32_t handleComponentCount = 6u;
class ZesEngineFixture : public SysmanDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<MockEngineNeoDrm> pDrm;
    std::unique_ptr<MockEnginePmuInterfaceImp> pPmuInterface;
    Drm *pOriginalDrm = nullptr;
    PmuInterface *pOriginalPmuInterface = nullptr;
    MemoryManager *pMemoryManagerOriginal = nullptr;
    std::unique_ptr<MockMemoryManagerInEngineSysman> pMemoryManager;
    std::unique_ptr<MockEngineSysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockEngineFsAccess> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pMemoryManagerOriginal = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = false;
        device->getDriverHandle()->setMemoryManager(pMemoryManager.get());

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEngineSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEngineFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pDrm = std::make_unique<MockEngineNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(neoDevice->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImp>(pLinuxSysmanImp);
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pDrm = pDrm.get();
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
        pLinuxSysmanImp->isUsingPrelimEnabledKmd = false;
        pFsAccess->mockReadVal = 23;

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }

        getEngineHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOriginal);
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;

        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_engine_handle_t> getEngineHandles(uint32_t count) {
        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

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

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(mockActiveTime / microSecondsToNanoSeconds, stats.activeTime);
        EXPECT_EQ(mockTimestamp / microSecondsToNanoSeconds, stats.timestamp);
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndDiscreteDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(mockActiveTime / microSecondsToNanoSeconds, stats.activeTime);
        EXPECT_EQ(mockTimestamp / microSecondsToNanoSeconds, stats.timestamp);
    }
}

TEST_F(ZesEngineFixture, GivenTestDiscreteDevicesAndValidEngineHandleWhenCallingZesEngineGetActivityAndPMUGetEventTypeFailsThenVerifyEngineGetActivityReturnsFailure) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadSymLinkError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto pOsEngineTest1 = OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);

    zes_engine_stats_t stats = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest1->getActivity(&stats));
    pFsAccess->mockReadVal = 0;
    pFsAccess->mockReadErrorVal = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto pOsEngineTest2 = OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest2->getActivity(&stats));
}

TEST_F(ZesEngineFixture, GivenTestIntegratedDevicesAndValidEngineHandleWhenCallingZesEngineGetActivityAndPMUGetEventTypeFailsThenVerifyEngineGetActivityReturnsFailure) {
    zes_engine_stats_t stats = {};
    pFsAccess->mockReadVal = 0;
    pFsAccess->mockReadErrorVal = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto pOsEngineTest1 = OsEngine::create(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 0u, 0u, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsEngineTest1->getActivity(&stats));
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndPmuReadFailsThenVerifyEngineGetActivityReturnsFailure) {
    pPmuInterface->mockPmuReadFailureReturnValue = -1;
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivity(handle, &stats));
    }
}

TEST_F(ZesEngineFixture, GivenPerfEventOpenFailsWhenEnumeratingHandlesThenFailureIsObserved) {
    pPmuInterface->mockPerfEventFailureReturnValue = -1;
    std::unique_ptr<LinuxEngineImp> engineImp = std::make_unique<LinuxEngineImp>(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 1, 0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, engineImp->isEngineModuleSupported());
}

TEST_F(ZesEngineFixture, GivenPerfEventOpenFailsWhenEnumeratingHandlesUnsupportedEngineClassIsPassedThenFailureIsObserved) {
    pPmuInterface->mockPerfEventFailureReturnValue = -1;
    std::unique_ptr<LinuxEngineImp> engineImp = std::make_unique<LinuxEngineImp>(pOsSysman, ZES_ENGINE_GROUP_RENDER_ALL, 1, 0, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, engineImp->isEngineModuleSupported());
}

TEST_F(ZesEngineFixture, GivenPerfEventOpenFailsBecauseOfHandlesUnavailableThenFailureIsObserved) {
    pPmuInterface->mockPerfEventFailureReturnValue = -1;
    {
        pPmuInterface->mockErrorNumber = EMFILE;
        std::unique_ptr<LinuxEngineImp> engineImp = std::make_unique<LinuxEngineImp>(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 1, 0, false);
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, engineImp->isEngineModuleSupported());
    }

    {
        pPmuInterface->mockErrorNumber = ENFILE;
        std::unique_ptr<LinuxEngineImp> engineImp = std::make_unique<LinuxEngineImp>(pOsSysman, ZES_ENGINE_GROUP_RENDER_SINGLE, 1, 0, false);
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, engineImp->isEngineModuleSupported());
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityExtThenVerifyFailureIsReturned) {
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        uint32_t count = 0;
        zes_engine_stats_t stats = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, &count, &stats));
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndperfEventOpenFailsThenVerifyEngineGetActivityReturnsFailure) {
    pPmuInterface->mockPerfEventFailureReturnValue = -1;
    MockEnginePmuInterfaceImp pPmuInterfaceImp(pLinuxSysmanImp);
    EXPECT_EQ(-1, pPmuInterface->pmuInterfaceOpen(0, -1, 0));
}

TEST_F(ZesEngineFixture, GivenValidOsSysmanPointerWhenRetrievingEngineTypeAndInstancesAndIfEngineInfoQueryFailsThenErrorIsReturned) {
    std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> engineGroupInstance;
    pDrm->mockSysmanQueryEngineInfoReturnFalse = false;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman));
}

TEST_F(ZesEngineFixture, givenEngineInfoQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsCreatedWithEngines) {
    auto drm = std::make_unique<DrmMockEngine>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    ASSERT_NE(nullptr, drm);
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->sysmanQueryEngineInfo();
    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->getEngineInfos().size());
}

TEST_F(ZesEngineFixture, GivenEngineInfoWithVideoQuerySupportedWhenQueryingEngineInfoWithVideoThenEngineInfoIsCreatedWithEngines) {
    auto drm = std::make_unique<DrmMockEngine>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    ASSERT_NE(nullptr, drm);
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->sysmanQueryEngineInfo();
    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->getEngineInfos().size());
}

TEST_F(ZesEngineFixture, GivenEngineInfoWithVideoQueryFailsThenFailureIsReturned) {
    auto drm = std::make_unique<DrmMockEngineInfoFailing>((const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    ASSERT_NE(nullptr, drm);
    EXPECT_FALSE(drm->sysmanQueryEngineInfo());
}

} // namespace ult
} // namespace L0
