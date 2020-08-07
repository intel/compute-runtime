/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

class SysmanKmdManagerFixture : public ::testing::Test {

  protected:
    Mock<MockKmdSysManager> *pKmdSysManager = nullptr;

    void SetUp() {
        pKmdSysManager = new Mock<MockKmdSysManager>;

        EXPECT_CALL(*pKmdSysManager, escape(_, _, _, _, _))
            .WillRepeatedly(::testing::Invoke(pKmdSysManager, &Mock<MockKmdSysManager>::mock_escape));
    }
    void TearDown() override {
        if (pKmdSysManager != nullptr) {
            delete pKmdSysManager;
            pKmdSysManager = nullptr;
        }
    }
};

TEST_F(SysmanKmdManagerFixture, CheckRequestSingleSucceedsAndPowerValueIsRetreivedCorrectlyAllowingSetToFalse) {
    pKmdSysManager->allowSetCalls = false;

    ze_result_t result = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    value = static_cast<uint32_t>(value);

    EXPECT_EQ(value, pKmdSysManager->mockPowerLimit1);
}

TEST_F(SysmanKmdManagerFixture, CheckRequestSingleSucceedsAndPowerValueIsRetreivedChangedSetThenRetreivedAgainCorrectlyAllowingSetToTrue) {
    pKmdSysManager->allowSetCalls = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    constexpr uint32_t increase = 500;
    uint32_t iniitialPl1 = pKmdSysManager->mockPowerLimit1;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = 0;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    value = static_cast<uint32_t>(value);

    EXPECT_EQ(value, iniitialPl1);

    value += increase;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = sizeof(uint32_t);

    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = 0;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    value = static_cast<uint32_t>(value);

    EXPECT_EQ(value, (iniitialPl1 + increase));
}

TEST_F(SysmanKmdManagerFixture, CheckRequestSingleGetCommandWithCorruptInformationVerifyCallFails) {
    pKmdSysManager->allowSetCalls = false;

    ze_result_t result = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = sizeof(uint64_t);

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    request.commandId = KmdSysman::Command::MaxCommands;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = 0;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MaxComponents;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = 0;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::MaxPowerRequests;
    request.dataSize = 0;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanKmdManagerFixture, CheckRequestSingleSetCommandWithCorruptInformationVerifyCallFails) {
    pKmdSysManager->allowSetCalls = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;
    uint32_t value = 0;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = 0;
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    request.commandId = KmdSysman::Command::MaxCommands;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
    request.dataSize = sizeof(uint32_t);

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MaxComponents;
    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::MaxPowerRequests;

    result = pKmdSysManager->requestSingle(request, response);

    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
