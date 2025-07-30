/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

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

TEST_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenRetrievingSysmanDeviceThenNonNullHandleIsReturned) {
    auto coreDeviceHandle = L0::Device::fromHandle(device->toHandle());
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(coreDeviceHandle);
    EXPECT_NE(pSysmanDevice, nullptr);
}

TEST_F(SysmanDeviceFixtureWithCore, GivenNullCoreHandleWhenSysmanHandleIsQueriedThenNullPtrIsReturned) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(nullptr);
    EXPECT_EQ(pSysmanDevice, nullptr);
}

TEST_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenSysmanHandleIsQueriedAndNotFoundInLookUpThenNullPtrIsReturned) {
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

TEST_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenRetrievingSysmanHandleTwiceThenSuccessAreReturned) {
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
