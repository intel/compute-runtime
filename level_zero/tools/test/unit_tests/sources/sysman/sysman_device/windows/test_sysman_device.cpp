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

ACTION_P(SetProcessList, value) {
    arg0 = value;
}

constexpr uint32_t pid1 = 1711u;
constexpr uint32_t pid2 = 1722u;
constexpr uint32_t pid3 = 1733u;
constexpr int64_t memSize1 = 0;
constexpr int64_t memSize2 = 0;
constexpr int64_t memSize3 = 0;
constexpr int64_t engines1 = 12u;
constexpr int64_t engines2 = 10u;
constexpr int64_t engines3 = 4u;

class SysmanSysmanDeviceFixture : public DeviceFixture, public ::testing::Test {

  protected:
    SysmanImp *sysmanImp;
    zet_sysman_handle_t hSysman;

    Mock<OsSysmanDevice> *pOsSysmanDevice;
    L0::SysmanDeviceImp *pSysmanDevice;
    L0::SysmanDevice *pSysmanDevice_prev;
    ze_device_handle_t hCoreDevice;

    zet_process_state_t mockProcessState1 = {pid1, memSize1, engines1};
    zet_process_state_t mockProcessState2 = {pid2, memSize2, engines2};
    zet_process_state_t mockProcessState3 = {pid3, memSize3, engines3};
    std::vector<zet_process_state_t> mockProcessStates{mockProcessState1, mockProcessState2, mockProcessState3};

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
        ON_CALL(*pOsSysmanDevice, scanProcessesState(_))
            .WillByDefault(DoAll(
                SetProcessList(mockProcessStates),
                Return(ZE_RESULT_SUCCESS)));

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

TEST_F(SysmanSysmanDeviceFixture, GivenValidSysmanHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenSuccessIsReturned) {
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanProcessesGetState(hSysman, &count, nullptr));
    EXPECT_EQ(count, 3u);
    std::vector<zet_process_state_t> processes(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanProcessesGetState(hSysman, &count, processes.data()));
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(processes[0].processId, pid1);
    EXPECT_EQ(processes[1].processId, pid2);
    EXPECT_EQ(processes[2].processId, pid3);
    EXPECT_EQ(processes[0].engines, engines1);
    EXPECT_EQ(processes[1].engines, engines2);
    EXPECT_EQ(processes[2].engines, engines3);
    EXPECT_EQ(processes[0].memSize, memSize1);
    EXPECT_EQ(processes[1].memSize, memSize2);
    EXPECT_EQ(processes[2].memSize, memSize3);
}

} // namespace ult
} // namespace L0
