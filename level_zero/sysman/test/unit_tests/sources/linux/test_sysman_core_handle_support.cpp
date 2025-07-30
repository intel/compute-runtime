/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanDeviceFixtureWithCore : public SysmanDeviceFixture, public L0::ult::DeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        L0::ult::DeviceFixture::setUp();
    }
    void TearDown() override {
        L0::ult::DeviceFixture::tearDown();
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenRetrievingSysmanDeviceThenNonNullHandleIsReturned, IsPVC) {
    auto coreDeviceHandle = L0::Device::fromHandle(device->toHandle());
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(coreDeviceHandle);
    EXPECT_NE(pSysmanDevice, nullptr);
}

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenNullCoreHandleWhenSysmanHandleIsQueriedThenNullPtrIsReturned, IsPVC) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(nullptr);
    EXPECT_EQ(pSysmanDevice, nullptr);
}

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenSysmanHandleIsQueriedAndNotFoundInLookUpThenNullPtrIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
        return buf;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
        std::memcpy(buf, str.c_str(), str.size());
        return static_cast<int>(str.size());
    });

    class MockSysmanDriverHandleImp : public SysmanDriverHandleImp {
      public:
        MockSysmanDriverHandleImp() {
            SysmanDriverHandleImp();
        }
        void clearUuidDeviceMap() {
            uuidDeviceMap.clear();
        }
    };
    std::unique_ptr<MockSysmanDriverHandleImp> pSysmanDriverHandleImp = std::make_unique<MockSysmanDriverHandleImp>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->initialize(*(L0::Sysman::ult::SysmanDeviceFixture::execEnv)));
    pSysmanDriverHandleImp->clearUuidDeviceMap();
    auto pSysmanDevice = pSysmanDriverHandleImp->getSysmanDeviceFromCoreDeviceHandle(L0::Device::fromHandle(device->toHandle()));
    EXPECT_EQ(pSysmanDevice, nullptr);
}

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenRetrievingSysmanHandleTwiceThenSuccessAreReturned, IsDG2) {
    auto coreDeviceHandle = L0::Device::fromHandle(device->toHandle());
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(coreDeviceHandle);
    EXPECT_NE(pSysmanDevice, nullptr);
    pSysmanDevice = nullptr;
    pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(coreDeviceHandle);
    EXPECT_NE(pSysmanDevice, nullptr);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
