/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysman_device.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

ACTION_P(SetString, value) {
    for (int i = 0; i < ZET_STRING_PROPERTY_SIZE; i++) {
        arg0[i] = value[i];
    }
}

class SysmanSysmanDeviceFixture : public DeviceFixture, public ::testing::Test {

  protected:
    SysmanImp *sysmanImp;
    zet_sysman_handle_t hSysman;

    Mock<OsSysmanDevice> *pOsSysmanDevice;
    L0::SysmanDeviceImp *pSysmanDevice;
    L0::SysmanDevice *pSysmanDevice_prev;
    ze_device_handle_t hCoreDevice;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = new SysmanImp(device->toHandle());
        pOsSysmanDevice = new NiceMock<Mock<OsSysmanDevice>>;

        hCoreDevice = device->toHandle();
        pSysmanDevice = new SysmanDeviceImp();
        pSysmanDevice->hCoreDevice = hCoreDevice;
        pSysmanDevice->pOsSysmanDevice = pOsSysmanDevice;

        ON_CALL(*pOsSysmanDevice, getSerialNumber(_))
            .WillByDefault(DoAll(
                SetString("validserialnumber"),
                Return()));
        ON_CALL(*pOsSysmanDevice, getBoardNumber(_))
            .WillByDefault(DoAll(
                SetString("validboardnumber"),
                Return()));
        ON_CALL(*pOsSysmanDevice, getBrandName(_))
            .WillByDefault(DoAll(
                SetString("intel"),
                Return()));
        ON_CALL(*pOsSysmanDevice, getModelName(_))
            .WillByDefault(DoAll(
                SetString("skl"),
                Return()));
        ON_CALL(*pOsSysmanDevice, getVendorName(_))
            .WillByDefault(DoAll(
                SetString("intel"),
                Return()));
        ON_CALL(*pOsSysmanDevice, getDriverVersion(_))
            .WillByDefault(DoAll(
                SetString("5.0.0-37-generic SMP mod_unload"),
                Return()));
        ON_CALL(*pOsSysmanDevice, reset())
            .WillByDefault(Return(ZE_RESULT_SUCCESS));

        pSysmanDevice_prev = sysmanImp->pSysmanDevice;
        sysmanImp->pSysmanDevice = static_cast<L0::SysmanDevice *>(pSysmanDevice);

        hSysman = sysmanImp->toHandle();
    }

    void TearDown() override {
        // restore state
        sysmanImp->pSysmanDevice = pSysmanDevice_prev;
        // cleanup
        if (pSysmanDevice != nullptr) {
            delete pSysmanDevice;
            pSysmanDevice = nullptr;
        }
        if (sysmanImp != nullptr) {
            delete sysmanImp;
            sysmanImp = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanSysmanDeviceFixture, GivenValidSysmanHandleWhenCallingzetSysmanDeviceGetPropertiesThenVerifyzetSysmanDeviceGetPropertiesCallSucceeds) {
    zet_sysman_properties_t properties;
    ze_result_t result = zetSysmanDeviceGetProperties(hSysman, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
