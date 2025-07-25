/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"
#include "level_zero/sysman/source/api/engine/sysman_engine_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/engine/linux/mock_engine_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/mock_pmu_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockEngineHandleCount = 9u;

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
        pDrm->ioctlHelper = std::make_unique<NEO::IoctlHelperXe>(*pDrm);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockNeoDrm>(pDrm));

        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pDrm);

        pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
        pPmuInterface->mockPmuFd = 10;
        pPmuInterface->mockActiveTime = 98765432;
        pPmuInterface->mockTimestamp = 8765432;
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

HWTEST2_F(SysmanProductHelperEngineXeTestFixture, GivenComponentCountZeroWhenCallingZesDeviceEnumEngineGroupsThenCallSucceedsAndValidCountIsReturned, IsBMG) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint32_t mockReadVal = 23;
        std::ostringstream oStream;
        oStream << mockReadVal;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockEngineHandleCount);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, mockEngineHandleCount);

    count = 0;
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockEngineHandleCount);
}

HWTEST2_F(SysmanProductHelperEngineXeTestFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityThenCallSuccedsAndValidValuesAreReturned, IsBMG) {

    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(mockEngineHandleCount);
    EXPECT_EQ(mockEngineHandleCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(zesEngineGetActivity(handle, &stats), ZE_RESULT_SUCCESS);
        EXPECT_EQ(stats.activeTime, pPmuInterface->mockActiveTime);
        EXPECT_EQ(stats.timestamp, pPmuInterface->mockTimestamp);
    }
}

HWTEST2_F(SysmanProductHelperEngineXeTestFixture, GivenValidEngineHandleAndPmuTimeStampIsZeroWhenCallingZesEngineGetActivityThenValidTimeStampIsReturned, IsBMG) {
    zes_engine_stats_t stats = {};
    pPmuInterface->mockTimestamp = 0u;
    auto handles = getEngineHandles(mockEngineHandleCount);
    EXPECT_EQ(mockEngineHandleCount, handles.size());

    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    uint64_t timeBeforeApiCall = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(zesEngineGetActivity(handle, &stats), ZE_RESULT_SUCCESS);
        EXPECT_EQ(stats.activeTime, pPmuInterface->mockActiveTime);
        EXPECT_GE(stats.timestamp, timeBeforeApiCall);
    }
}

HWTEST2_F(SysmanProductHelperEngineXeTestFixture, GivenValidEngineHandleAndPmuReadFailsWhenCallingZesEngineGetActivityThenErrorIsReturned, IsBMG) {

    zes_engine_stats_t stats = {};
    pPmuInterface->mockPmuReadFailureReturnValue = -1;
    auto handles = getEngineHandles(mockEngineHandleCount);
    EXPECT_EQ(mockEngineHandleCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(zesEngineGetActivity(handle, &stats), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanProductHelperEngineXeTestFixture, GivenSysmanProductHelperHandleWhenCheckingIsAggregationOfSingleEnginesSupportedThenSuccessIsReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isAggregationOfSingleEnginesSupported());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
