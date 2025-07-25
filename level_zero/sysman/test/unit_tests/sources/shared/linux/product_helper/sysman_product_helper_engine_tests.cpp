/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/engine/linux/mock_engine_prelim.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t handleCountForMultiDeviceFixture = 7u;

class SysmanProductHelperEngineTestsFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockEnginePmuInterfaceImpPrelim> pPmuInterface;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });

        MockEngineNeoDrmPrelim *pDrm = new MockEngineNeoDrmPrelim(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockEngineNeoDrmPrelim>(pDrm));

        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceI915Prelim(pLinuxSysmanImp->getSysmanProductHelper()));
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pDrm);

        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImpPrelim>(pLinuxSysmanImp);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pDrm->mockReadSysmanQueryEngineInfoMultiDevice = true;

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        bool isIntegratedDevice = true;
        pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
        device = pSysmanDevice;
        getEngineHandles(0);
    }

    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
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

HWTEST2_F(SysmanProductHelperEngineTestsFixture, GivenComponentCountZeroWhenCallingZesDeviceEnumEngineGroupsThenSuccessAndNonZeroCountIsReturned, IsPVC) {
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

HWTEST2_F(SysmanProductHelperEngineTestsFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenSuccessIsReturned, IsPVC) {
    zes_engine_properties_t properties;
    auto handles = getEngineHandles(handleCountForMultiDeviceFixture);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[4], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[5], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[6], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
}

HWTEST2_F(SysmanProductHelperEngineTestsFixture, GivenHandleQueryItemCalledAndPmuInterfaceOpenFailsWhenCallingZesDeviceEnumEngineGroupsThenSuccessAndZeroHandleCountIsReturned, IsPVC) {

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(pOsSysman->getSubDeviceCount());
    uint32_t count = 0;
    uint32_t mockHandleCount = 0u;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);
}

HWTEST2_F(SysmanProductHelperEngineTestsFixture, GivenValidEngineHandleWith3DSingleGroupEngineWhenCallingEngineGetActivityThenErrorIsReturned, IsPVC) {
    zes_engine_group_t engineType = ZES_ENGINE_GROUP_3D_SINGLE;
    zes_engine_stats_t stats = {};

    auto pLinuxEngineImp = std::make_unique<L0::Sysman::LinuxEngineImp>(pOsSysman, engineType, 0, 0, 0);
    EXPECT_EQ(pLinuxEngineImp->getActivity(&stats), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperEngineTestsFixture, GivenSysmanProductHelperHandleWhenCheckingIsAggregationOfSingleEnginesSupportedThenFailureIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isAggregationOfSingleEnginesSupported());
}

HWTEST2_F(SysmanProductHelperEngineTestsFixture, GivenSysmanProductHelperHandleWhenCallingGetGroupEngineBusynessFromSingleEnginesThenErrorIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_engine_group_t engineType = ZES_ENGINE_GROUP_3D_SINGLE;
    zes_engine_stats_t stats = {};
    EXPECT_EQ(pSysmanProductHelper->getGroupEngineBusynessFromSingleEngines(pLinuxSysmanImp, &stats, engineType), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
