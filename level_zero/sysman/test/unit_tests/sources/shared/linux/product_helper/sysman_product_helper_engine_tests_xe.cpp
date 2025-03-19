/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/engine/linux/mock_engine_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/mock_pmu_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockEngineHandleCount = 4u;

class SysmanProductHelperEngineXeTestFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockPmuInterfaceImp> pPmuInterface;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });
        device = pSysmanDevice;
        MockNeoDrm *pDrm = new MockNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = std::make_unique<MockIoctlHelperXe>(*pDrm);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockNeoDrm>(pDrm));

        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pDrm);

        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pPmuInterface->mockPmuFd = 10;
        pPmuInterface->pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        bool isIntegratedDevice = true;
        pLinuxSysmanImp->pSysmanKmdInterface->setSysmanDeviceDirName(isIntegratedDevice);
    }

    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanProductHelperEngineXeTestFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenSuccessIsReturned, IsBMG) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint32_t mockReadVal = 23;
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t count = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, mockEngineHandleCount);
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()));
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }

    zes_engine_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COMPUTE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COPY_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
