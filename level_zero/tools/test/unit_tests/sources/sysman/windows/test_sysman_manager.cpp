/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman_const.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

#include "gtest/gtest.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

uint64_t convertTStoMicroSec(uint64_t ts, uint32_t freq) {
    double timeFactor = 1.0 / static_cast<double>(freq);
    timeFactor = static_cast<double>(ts) * timeFactor;
    timeFactor *= static_cast<double>(microFacor);
    return static_cast<uint64_t>(timeFactor);
}

class SysmanKmdManagerFixture : public ::testing::Test {

  protected:
    Mock<MockKmdSysManager> *pKmdSysManager = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pKmdSysManager = new Mock<MockKmdSysManager>;
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        if (pKmdSysManager != nullptr) {
            delete pKmdSysManager;
            pKmdSysManager = nullptr;
        }
    }
};

TEST_F(SysmanKmdManagerFixture, GivenAllowSetCallsFalseWhenRequestingSingleThenPowerValueIsCorrect) {
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

TEST_F(SysmanKmdManagerFixture, GivenAllowSetCallsTrueWhenRequestingSingleThenPowerValueIsCorrect) {
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

TEST_F(SysmanKmdManagerFixture, GivenAllowSetCallsFalseAndCorruptedDataWhenRequestingSingleThenCallFails) {
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

TEST_F(SysmanKmdManagerFixture, GivenAllowSetCallsTrueAndCorruptedDataWhenRequestingSingleThenCallFails) {
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

TEST_F(SysmanKmdManagerFixture, GivenAllowSetCallsFalseAndTDROccurredWhenRequestSingleIsCalledThenErrorDeviceLostIsReturned) {
    pKmdSysManager->allowSetCalls = false;
    pKmdSysManager->mockEscapeResult = STATUS_DEVICE_REMOVED;

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

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

TEST_F(SysmanKmdManagerFixture, GivenAllowSetCallsFalseAndTDROccurredWhenRequestMultipleIsCalledThenErrorDeviceLostIsReturned) {
    pKmdSysManager->allowSetCalls = false;
    pKmdSysManager->mockEscapeResult = STATUS_DEVICE_REMOVED;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;

    request.requestId = KmdSysman::Requests::Memory::MaxBandwidth;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::CurrentBandwidthRead;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::CurrentBandwidthWrite;
    vRequests.push_back(request);

    result = pKmdSysManager->requestMultiple(vRequests, vResponses);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

} // namespace ult
} // namespace L0
