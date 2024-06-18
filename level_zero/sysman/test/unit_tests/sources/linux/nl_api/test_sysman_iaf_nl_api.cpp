/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/shared/linux/nl_api/sysman_iaf_nl_api.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_nl_api.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <limits>
#include <netlink/handlers.h>

struct nl_sock {};

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
}

namespace L0 {
namespace Sysman {
namespace ult {

class PublicIafNlApi : public L0::Sysman::IafNlApi {
  public:
    using IafNlApi::cleanup;
    using IafNlApi::init;
    using IafNlApi::pNlApi;
};

class SysmanIafNlApiFixture : public ::testing::Test {
  protected:
    PublicIafNlApi testIafNlApi;

    void SetUp() override {
        auto mockNlApi = std::make_unique<MockNlApi>();

        testIafNlApi.pNlApi = std::move(mockNlApi);
    }
    void TearDown() override {
    }
    void setupExpectCommand(uint16_t cmd, uint32_t fabricId, uint32_t attachId, uint8_t portId);
    void setupExpectCommand(uint16_t cmd, uint32_t fabricId, uint32_t attachId);
    void setupExpectCommand(uint16_t cmd, uint32_t fabricId);
    void setupExpectCommand(uint16_t cmd);
    void setupExpectInit();
    void setupExpectCleanup();
    void setupExpectInitMultiple();
    void setupExpectCleanupMultiple();

  public:
    struct nl_sock nlSock;
    struct genl_ops *pOps;
    static const char *iafFamilyName;
};

const char *SysmanIafNlApiFixture::iafFamilyName = "iaf_ze";

void SysmanIafNlApiFixture::setupExpectCommand(uint16_t cmd, uint32_t fabricId, uint32_t attachId, uint8_t portNumber) {
    setupExpectCommand(cmd, fabricId, attachId);
}

void SysmanIafNlApiFixture::setupExpectCommand(uint16_t cmd, uint32_t fabricId, uint32_t attachId) {
    setupExpectCommand(cmd, fabricId);
}

void SysmanIafNlApiFixture::setupExpectCommand(uint16_t cmd, uint32_t fabricId) {
    setupExpectCommand(cmd);
}

void SysmanIafNlApiFixture::setupExpectCommand(uint16_t cmd) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockNlSendAutoReturnValue.push_back(0);
}

void SysmanIafNlApiFixture::setupExpectInit() {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pMockNlApi->mockGenlConnectReturnValue.push_back(0);
    pMockNlApi->mockGenCtrlResolveReturnValue.push_back(0);
    pMockNlApi->mockNlSocketModifyCbReturnValue.push_back(0);
}

void SysmanIafNlApiFixture::setupExpectInitMultiple() {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pMockNlApi->mockGenlConnectReturnValue.push_back(0);
    pMockNlApi->mockGenCtrlResolveReturnValue.push_back(0);
    pMockNlApi->mockNlSocketModifyCbReturnValue.push_back(0);
    pMockNlApi->isRepeated = true;
}

void SysmanIafNlApiFixture::setupExpectCleanupMultiple() {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockGenlUnregisterFamilyReturnValue.push_back(0);
    pMockNlApi->isRepeated = true;
}

void SysmanIafNlApiFixture::setupExpectCleanup() {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockGenlUnregisterFamilyReturnValue.push_back(0);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenLoadEntryPointsFailsThenInitRepeatedlyReturnsDependencyUnavailable) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.init());
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGenlRegisterFamilyFailsWithExistThenInitReturnsNotReady) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(-NLE_EXIST);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGenlRegisterFamilyFailsWithAnyOtherErrorInitReturnsErrorUnknown) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(-NLE_FAILURE);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenNlSocketAllocFailsThenInitReturnsErrorUnknown) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGenlConnectFailsThenInitReturnsErrorUnknown) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pMockNlApi->mockGenlConnectReturnValue.push_back(-1);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGenlOpsResolveFailsThenInitReturnsErrorUnknown) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pMockNlApi->mockGenlConnectReturnValue.push_back(0);
    pMockNlApi->mockGenlOpsResolveReturnValue.push_back(-1);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGenlCtrlResolveFailsThenInitReturnsErrorUnknown) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pMockNlApi->mockGenlConnectReturnValue.push_back(0);
    pMockNlApi->mockGenlOpsResolveReturnValue.push_back(0);
    pMockNlApi->mockGenCtrlResolveReturnValue.push_back(-1);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenNlSocketModifyCbFailsThenInitReturnsErrorUnknown) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pMockNlApi->mockGenlConnectReturnValue.push_back(0);
    pMockNlApi->mockGenlOpsResolveReturnValue.push_back(0);
    pMockNlApi->mockGenCtrlResolveReturnValue.push_back(0);
    pMockNlApi->mockNlSocketModifyCbReturnValue.push_back(-1);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenAllNlApisReturnSuccessThenInitReturnsSuccess) {
    setupExpectInit();
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.init());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenInitSucceedsThenCleanupSucceeds) {
    setupExpectInit();
    setupExpectCleanup();
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.init());
    testIafNlApi.cleanup();
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenInitCalledMultipleTimesThenLoadEntryPointsOnlyCalledOnce) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);
    pMockNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pMockNlApi->mockGenlConnectReturnValue.push_back(0);
    pMockNlApi->mockGenlOpsResolveReturnValue.push_back(0);
    pMockNlApi->mockGenCtrlResolveReturnValue.push_back(0);
    pMockNlApi->mockNlSocketModifyCbReturnValue.push_back(0);
    pMockNlApi->isRepeated = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.init());
    testIafNlApi.cleanup();
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.init());
    testIafNlApi.cleanup();
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledThenReturnSuccess) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_REM_REQUEST);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndInitFailsThenErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(false);

    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndNlmsgAllocFailsThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenCallingHandleResponseWithInvalidCommandThenUnknownErrorReturned) {
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.handleResponse(_IAF_CMD_OP_COUNT + 1, nullptr, nullptr));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndPerformTransactionRecvmsgsDefaultFailedThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlSendAutoReturnValue.push_back(0);
    pMockNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(-NLE_FAILURE);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndPerformTransactionRecvmsgsDefaulFailsWith_NLE_PERM_ThenInsufficientPermissionsReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlSendAutoReturnValue.push_back(0);
    pMockNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(-NLE_PERM);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndNlOperationFailsWhenResponseTypeNotResponseThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_REM_REQUEST);
    pMockNlApi->cmdMsgType = IAF_CMD_MSG_REQUEST;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndNlOperationFailsWhenOpResultNotSuccessThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_REM_REQUEST);
    pMockNlApi->cmdRspRslt = IAF_CMD_RSP_FAILURE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndNlOperationFailsWhenResponseTypeNotPresentThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_REM_REQUEST);
    pMockNlApi->cmdMsgType = _IAF_CMD_MSG_TYPE_COUNT;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndNlOperationFailsWhenOpResultNotPresentThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_REM_REQUEST);
    pMockNlApi->cmdRspRslt = _IAF_CMD_RSP_COUNT;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndPerformTransactionNlSendAutoFailedThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlSendAutoReturnValue.push_back(-1);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndNlOperationFailsWhenMakeContextNotMatchThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_REM_REQUEST);
    pMockNlApi->cmdMakeContextNotMatch = true;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndNlOperationFailsWhenMakeContextNotPresentThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->cmdMakeContextNotPresent = true;
    pMockNlApi->mockNlSendAutoReturnValue.push_back(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndAllocMsgFailsWhenGenlmsgPutReturnNullPtrThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockGenlmsgPutReturnValue.push_back(nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRemRequestCalledAndUnexpectedCmdCompletesThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockGenlmsgPutReturnValue.push_back(pMockNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.remRequest());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortBeaconEnableCalledThenReturnSuccess) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_BEACON_ENABLE, MockNlApi::testPortId.fabricId, MockNlApi::testPortId.attachId, MockNlApi::testPortId.portNumber);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portBeaconEnable(MockNlApi::testPortId));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortBeaconDisableCalledThenReturnSuccess) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_BEACON_DISABLE, MockNlApi::testPortId.fabricId, MockNlApi::testPortId.attachId, MockNlApi::testPortId.portNumber);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portBeaconDisable(MockNlApi::testPortId));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortEnableCalledThenReturnSuccess) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_ENABLE, MockNlApi::testPortId.fabricId, MockNlApi::testPortId.attachId, MockNlApi::testPortId.portNumber);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portEnable(MockNlApi::testPortId));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortDisableCalledThenReturnSuccess) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_DISABLE, MockNlApi::testPortId.fabricId, MockNlApi::testPortId.attachId, MockNlApi::testPortId.portNumber);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portDisable(MockNlApi::testPortId));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortUsageEnableCalledThenReturnSuccess) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_USAGE_ENABLE, MockNlApi::testPortId.fabricId, MockNlApi::testPortId.attachId, MockNlApi::testPortId.portNumber);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portUsageEnable(MockNlApi::testPortId));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortUsageDisableCalledThenReturnSuccess) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_USAGE_DISABLE, MockNlApi::testPortId.fabricId, MockNlApi::testPortId.attachId, MockNlApi::testPortId.portNumber);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portUsageDisable(MockNlApi::testPortId));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRoutingGenQueryCalledThenBeginAndEndReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_ROUTING_GEN_QUERY);
    uint32_t start = std::numeric_limits<uint32_t>::max(), end = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.routingGenQuery(start, end));
    EXPECT_EQ(pMockNlApi->genStart, start);
    EXPECT_EQ(pMockNlApi->genEnd, end);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRoutingGenQueryCalledAndStartAndEndNotPresentThenStartAndEndZeroReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_ROUTING_GEN_QUERY);
    uint32_t start = std::numeric_limits<uint32_t>::max(), end = std::numeric_limits<uint32_t>::max();
    pMockNlApi->addRoutingGenStartEnd = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.routingGenQuery(start, end));
    EXPECT_EQ(0U, start);
    EXPECT_EQ(0U, end);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRoutingGenQueryCalledAndLoadEntryPointsFailsThenErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    uint32_t start = std::numeric_limits<uint32_t>::max(), end = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.routingGenQuery(start, end));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenRoutingGenQueryCalledAndNlmsgAllocFailsThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    uint32_t start = std::numeric_limits<uint32_t>::max(), end = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.routingGenQuery(start, end));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetThroughputCalledThenRxCounterAndTxCounterReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_XMIT_RECV_COUNTS, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    L0::Sysman::IafPortThroughPut throughput;
    throughput.txCounter = std::numeric_limits<uint64_t>::max();
    throughput.rxCounter = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.getThroughput(MockNlApi::testPortId, throughput));
    EXPECT_EQ(pMockNlApi->txCounter, throughput.txCounter);
    EXPECT_EQ(pMockNlApi->rxCounter, throughput.rxCounter);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetThroughputCalledAndRxAndTxCountersNotPresentThenRxAndTxCountersOfZeroReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_XMIT_RECV_COUNTS, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    L0::Sysman::IafPortThroughPut throughput;
    throughput.txCounter = std::numeric_limits<uint64_t>::max();
    throughput.rxCounter = std::numeric_limits<uint64_t>::max();
    pMockNlApi->addRxTxCounters = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.getThroughput(pMockNlApi->testPortId, throughput));
    EXPECT_EQ(0U, throughput.txCounter);
    EXPECT_EQ(0U, throughput.rxCounter);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetMultiPortThroughputCalledThenRxCounterAndTxCounterReturned) {

    std::vector<L0::Sysman::IafPortId> iafPortIdList = {};
    std::vector<L0::Sysman::IafThroughPutInfo> iafThroughPutList = {};
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_THROUGHPUT, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    pMockNlApi->testMultiPortThroughputCount = 3;
    for (auto i = 0U; i < pMockNlApi->testMultiPortThroughputCount; i++) {
        IafPortId iafPortId(pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber + i);
        iafPortIdList.push_back(iafPortId);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.getMultiPortThroughPut(iafPortIdList, iafThroughPutList));
    EXPECT_EQ(iafPortIdList.size(), iafThroughPutList.size());
    for (auto i = 0U; i < iafThroughPutList.size(); i++) {
        EXPECT_EQ(pMockNlApi->txCounter, iafThroughPutList.at(i).iafThroughput.txCounter);
        EXPECT_EQ(pMockNlApi->rxCounter, iafThroughPutList.at(i).iafThroughput.rxCounter);
        EXPECT_EQ(pMockNlApi->testPortId.fabricId, iafThroughPutList.at(i).iafPortId.fabricId);
        EXPECT_EQ(pMockNlApi->testPortId.attachId, iafThroughPutList.at(i).iafPortId.attachId);
        EXPECT_EQ(pMockNlApi->testPortId.portNumber + i, iafThroughPutList.at(i).iafPortId.portNumber);
    }
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetMultiPortThroughputCalledAndRxAndTxCountersNotPresentThenThroughputInfoSizeIsZero) {

    std::vector<L0::Sysman::IafPortId> iafPortIdList = {};
    std::vector<L0::Sysman::IafThroughPutInfo> iafThroughPutList = {};
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_THROUGHPUT, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    pMockNlApi->testMultiPortThroughputCount = 3;
    for (auto i = 0U; i < pMockNlApi->testMultiPortThroughputCount; i++) {
        IafPortId iafPortId(pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber + i);
        iafPortIdList.push_back(iafPortId);
    }

    pMockNlApi->addRxTxCounters = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.getMultiPortThroughPut(iafPortIdList, iafThroughPutList));
    EXPECT_EQ(0UL, iafThroughPutList.size());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetMultiPortThroughputCalledAndNlmsgAllocFailsThenUnknownErrorIsReturned) {
    std::vector<L0::Sysman::IafPortId> iafPortIdList = {};
    std::vector<L0::Sysman::IafThroughPutInfo> iafThroughPutList = {};
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    for (auto i = 0U; i < pMockNlApi->testMultiPortThroughputCount; i++) {
        IafPortId iafPortId(pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber + i);
        iafPortIdList.push_back(iafPortId);
    }

    pMockNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.getMultiPortThroughPut(iafPortIdList, iafThroughPutList));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetMultiPortThroughputCalledAndInitFailsThenErrorIsReturned) {
    std::vector<L0::Sysman::IafPortId> iafPortIdList = {};
    std::vector<L0::Sysman::IafThroughPutInfo> iafThroughPutList = {};
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    for (auto i = 0U; i < pMockNlApi->testMultiPortThroughputCount; i++) {
        IafPortId iafPortId(pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber + i);
        iafPortIdList.push_back(iafPortId);
    }

    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.getMultiPortThroughPut(iafPortIdList, iafThroughPutList));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenDeviceEnumCalledThenFabricIdsReturned) {
    std::vector<uint32_t> fabricIds;

    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_DEVICE_ENUM);
    fabricIds.clear();
    MockNlApi *mockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    mockNlApi->testFabricIds.clear();
    mockNlApi->testFabricIds.push_back(mockNlApi->testPortId.fabricId);
    mockNlApi->testFabricIds.push_back(mockNlApi->testPortId.fabricId + 1U);
    mockNlApi->testFabricIds.push_back(mockNlApi->testPortId.fabricId + 2U);
    mockNlApi->testFabricIds.push_back(mockNlApi->testPortId.fabricId + 3U);
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.deviceEnum(fabricIds));
    EXPECT_EQ(mockNlApi->testFabricIds.size(), fabricIds.size());
    for (auto i = 0U; i < mockNlApi->testFabricIds.size(); i++) {
        EXPECT_EQ(mockNlApi->testFabricIds.at(i), fabricIds.at(i));
    }
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFPortStatusQueryCalledThenPortStateReturned) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_STATUS_QUERY, MockNlApi::testPortId.fabricId,
                       MockNlApi::testPortId.attachId, MockNlApi::testPortId.portNumber);
    L0::Sysman::IafPortState state = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.fPortStatusQuery(MockNlApi::testPortId, state));
    EXPECT_EQ(IAF_FPORT_HEALTH_HEALTHY, static_cast<iaf_fport_health>(state.healthStatus));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFabricDevicePropertiesCalledThenSubDeviceCountReturned) {
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FABRIC_DEVICE_PROPERTIES, MockNlApi::testPortId.fabricId);
    uint32_t numSubDevices = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.fabricDeviceProperties(MockNlApi::testPortId.fabricId, numSubDevices));
    EXPECT_EQ(2U, numSubDevices);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFabricDevicePropertiesCalledAndSubDeviceCountNotPresentThenZeroSubDevicesReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FABRIC_DEVICE_PROPERTIES, pMockNlApi->testPortId.fabricId);
    uint32_t numSubDevices = 0;
    pMockNlApi->addSubDeviceCount = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.fabricDeviceProperties(MockNlApi::testPortId.fabricId, numSubDevices));
    EXPECT_EQ(0U, numSubDevices);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFabricDevicePropertiesCalledAndLoadEntryPointsFailsThenErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(false);

    uint32_t numSubDevices = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.fabricDeviceProperties(MockNlApi::testPortId.fabricId, numSubDevices));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFabricDevicePropertiesCalledAndNlmsgAllocFailsThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    uint32_t numSubDevices = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.fabricDeviceProperties(MockNlApi::testPortId.fabricId, numSubDevices));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFportPropertiesCalledThenPropertiesReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_PROPERTIES, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    uint64_t neighborGuid;
    uint8_t neighborPortNumber;
    L0::Sysman::IafPortSpeed maxRxSpeed;
    L0::Sysman::IafPortSpeed maxTxSpeed;
    L0::Sysman::IafPortSpeed rxSpeed;
    L0::Sysman::IafPortSpeed txSpeed;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.fportProperties(pMockNlApi->testPortId, neighborGuid, neighborPortNumber,
                                                              maxRxSpeed, maxTxSpeed, rxSpeed, txSpeed));
    EXPECT_EQ(neighborGuid, pMockNlApi->testGuid);
    EXPECT_EQ(neighborPortNumber, pMockNlApi->testPortId.portNumber);
    EXPECT_EQ(maxRxSpeed.bitRate, pMockNlApi->linkSpeed90g);
    EXPECT_EQ(maxRxSpeed.width, pMockNlApi->testLinkWidth1x);
    EXPECT_EQ(maxTxSpeed.bitRate, pMockNlApi->linkSpeed90g);
    EXPECT_EQ(maxTxSpeed.width, pMockNlApi->testLinkWidth1x);
    EXPECT_EQ(rxSpeed.bitRate, pMockNlApi->linkSpeed90g);
    EXPECT_EQ(rxSpeed.width, pMockNlApi->testLinkWidth3x);
    EXPECT_EQ(txSpeed.bitRate, pMockNlApi->linkSpeed90g);
    EXPECT_EQ(txSpeed.width, pMockNlApi->testLinkWidth4x);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFportPropertiesCalledAndInvalidWidthAndBitRate0DetectedThenNegativeOneReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_PROPERTIES, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    uint64_t neighborGuid;
    uint8_t neighborPortNumber;
    L0::Sysman::IafPortSpeed maxRxSpeed;
    L0::Sysman::IafPortSpeed maxTxSpeed;
    L0::Sysman::IafPortSpeed rxSpeed;
    L0::Sysman::IafPortSpeed txSpeed;
    pMockNlApi->useInvalidWidth = true;
    pMockNlApi->linkSpeed = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.fportProperties(pMockNlApi->testPortId, neighborGuid, neighborPortNumber,
                                                              maxRxSpeed, maxTxSpeed, rxSpeed, txSpeed));
    EXPECT_EQ(neighborGuid, pMockNlApi->testGuid);
    EXPECT_EQ(neighborPortNumber, pMockNlApi->testPortId.portNumber);
    EXPECT_EQ(maxRxSpeed.bitRate, -1);
    EXPECT_EQ(maxRxSpeed.width, -1);
    EXPECT_EQ(maxTxSpeed.bitRate, -1);
    EXPECT_EQ(maxTxSpeed.width, -1);
    EXPECT_EQ(rxSpeed.bitRate, -1);
    EXPECT_EQ(rxSpeed.width, -1);
    EXPECT_EQ(txSpeed.bitRate, -1);
    EXPECT_EQ(txSpeed.width, -1);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFportPropertiesCalledAndLoadEntryPointsFailsThenErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(false);

    uint64_t neighborGuid;
    uint8_t neighborPortNumber;
    L0::Sysman::IafPortSpeed maxRxSpeed;
    L0::Sysman::IafPortSpeed maxTxSpeed;
    L0::Sysman::IafPortSpeed rxSpeed;
    L0::Sysman::IafPortSpeed txSpeed;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.fportProperties(pMockNlApi->testPortId, neighborGuid, neighborPortNumber,
                                                                                   maxRxSpeed, maxTxSpeed, rxSpeed, txSpeed));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFportPropertiesCalledAndNlmsgAllocFailsThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    uint64_t neighborGuid;
    uint8_t neighborPortNumber;
    L0::Sysman::IafPortSpeed maxRxSpeed;
    L0::Sysman::IafPortSpeed maxTxSpeed;
    L0::Sysman::IafPortSpeed rxSpeed;
    L0::Sysman::IafPortSpeed txSpeed;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.fportProperties(pMockNlApi->testPortId, neighborGuid, neighborPortNumber,
                                                                    maxRxSpeed, maxTxSpeed, rxSpeed, txSpeed));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortBeaconStateQueryCalledThenPortBeaconStateReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_BEACON_STATE_QUERY, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    bool enabled = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portBeaconStateQuery(pMockNlApi->testPortId, enabled));
    EXPECT_EQ(true, enabled);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortSubDevicePropertiesGetCalledThenGuidAndPortsReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_SUB_DEVICE_PROPERTIES_GET, pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId);
    uint64_t guid = 0;
    std::vector<uint8_t> ports;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.subdevicePropertiesGet(pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId,
                                                                     guid, ports));
    EXPECT_EQ(guid, pMockNlApi->testGuid);
    EXPECT_EQ(8U, ports.size());
    for (auto i = 0U; i < ports.size(); i++) {
        EXPECT_EQ(ports.at(i), i + 1);
    }
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortSubDevicePropertiesGetCalledAndGuidNotPresentThenGuidIsZeroAndPortsReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_SUB_DEVICE_PROPERTIES_GET, pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId);
    uint64_t guid = pMockNlApi->testGuid;
    std::vector<uint8_t> ports;
    pMockNlApi->addGUID = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.subdevicePropertiesGet(pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId,
                                                                     guid, ports));
    EXPECT_EQ(0U, guid);
    EXPECT_EQ(8U, ports.size());
    for (auto i = 0U; i < ports.size(); i++) {
        EXPECT_EQ(ports.at(i), i + 1);
    }
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortSubDevicePropertiesGetCalledAndAddPortZeroAndTypeDisconnectedThenGuidAndZeroPortsReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_SUB_DEVICE_PROPERTIES_GET, pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId);
    uint64_t guid = 0;
    std::vector<uint8_t> ports;
    pMockNlApi->addPortZeroAndTypeDisconnected = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.subdevicePropertiesGet(pMockNlApi->testPortId.fabricId, pMockNlApi->testPortId.attachId,
                                                                     guid, ports));
    EXPECT_EQ(guid, pMockNlApi->testGuid);
    EXPECT_EQ(0U, ports.size());
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortSubDevicePropertiesGetCalledAndLoadEntryPointsFailsThenErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockLoadEntryPointsReturnValue.push_back(false);

    uint64_t guid = 0;
    std::vector<uint8_t> ports;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, testIafNlApi.subdevicePropertiesGet(pMockNlApi->testPortId.fabricId,
                                                                                          pMockNlApi->testPortId.attachId, guid, ports));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortSubDevicePropertiesGetAndNlmsgAllocFailsThenUnknownErrorReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    pMockNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    uint64_t guid = 0;
    std::vector<uint8_t> ports;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, testIafNlApi.subdevicePropertiesGet(pMockNlApi->testPortId.fabricId,
                                                                           pMockNlApi->testPortId.attachId, guid, ports));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenPortStateQueryCalledThenPortStateReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_PORT_STATE_QUERY, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    bool enabled = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.portStateQuery(pMockNlApi->testPortId, enabled));
    EXPECT_EQ(true, enabled);
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFPortStatusQueryCalledAndHeathIsOutOfRangeThenPortStatusUnknownReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_STATUS_QUERY, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    L0::Sysman::IafPortState state = {};
    pMockNlApi->fportHealth = _IAF_FPORT_HEALTH_COUNT;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.fPortStatusQuery(pMockNlApi->testPortId, state));
    EXPECT_EQ(_IAF_FPORT_HEALTH_COUNT, static_cast<iaf_fport_health>(state.healthStatus));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenFPortStatusQueryCalledAndPortHealthOffThenPortStatusDisabledReturned) {
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    setupExpectInit();
    setupExpectCleanup();
    setupExpectCommand(IAF_CMD_OP_FPORT_STATUS_QUERY, pMockNlApi->testPortId.fabricId,
                       pMockNlApi->testPortId.attachId, pMockNlApi->testPortId.portNumber);
    L0::Sysman::IafPortState state = {};
    pMockNlApi->fportHealth = IAF_FPORT_HEALTH_OFF;
    EXPECT_EQ(ZE_RESULT_SUCCESS, testIafNlApi.fPortStatusQuery(pMockNlApi->testPortId, state));
    EXPECT_EQ(IAF_FPORT_HEALTH_OFF, static_cast<iaf_fport_health>(state.healthStatus));
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWithLegacyIafPortsWhenGetPortsIsCalledValidPortsAreReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/drm/card1/device/iaf.0/iaf_fabric_id",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string fabricId("0x10000000");
        memcpy(buf, fabricId.c_str(), fabricId.size());
        return fabricId.size();
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    std::vector<L0::Sysman::IafPort> ports;
    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetPortsIsCalledValidPortsAreReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"i915.iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string fabricId("0x10000000");
        memcpy(buf, fabricId.c_str(), fabricId.size());
        return fabricId.size();
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    std::vector<L0::Sysman::IafPort> ports;

    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->addSubDeviceCount = false;
    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenIafDirectoryIsUnavailableWhenGetPortsIsCalledThenUnSupportedFeatureIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iafX.0"}});
    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    std::vector<L0::Sysman::IafPort> ports;

    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->addSubDeviceCount = false;
    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenOpeningFabricIdFileFailsWhenGetPortsIsCalledThenDependencyUnavailableIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    std::vector<L0::Sysman::IafPort> ports;
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->addSubDeviceCount = false;
    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenReadingFabricIdFileFailsWhenGetPortsIsCalledThenDependencyUnavailableIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return 0;
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    std::vector<L0::Sysman::IafPort> ports;
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->addSubDeviceCount = false;
    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenIncorrectValueInFabricIdFileFailsWhenGetPortsIsCalledThenUnknownErrorIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string fabricId("0xFFFFFFFFFF");
        memcpy(buf, fabricId.c_str(), fabricId.size());
        return fabricId.size();
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    std::vector<L0::Sysman::IafPort> ports;
    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->addSubDeviceCount = false;
    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetPortsIsCalledAndfabricDevicePropertiesReturnsFailureThenUnknownErrorIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iaf.0"}});
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string fabricId("0x10000000");
        memcpy(buf, fabricId.c_str(), fabricId.size());
        return fabricId.size();
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.clear();
    pMockNlApi->mockGenlRegisterFamilyReturnValue.clear();
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.clear();

    std::vector<L0::Sysman::IafPort> ports;

    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(-6);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);

    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetPortsIsCalledAndSubdevicePropertiesGetReturnsFailureThenUnknownErrorIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iaf.0"}});
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string fabricId("0x10000000");
        memcpy(buf, fabricId.c_str(), fabricId.size());
        return fabricId.size();
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.clear();
    pMockNlApi->mockGenlRegisterFamilyReturnValue.clear();
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.clear();

    std::vector<L0::Sysman::IafPort> ports;

    // Override the registerfamily to return error
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);

    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(-6);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);
    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanIafNlApiFixture, GivenIafNlApiWhenGetPortsIsCalledAndFportPropertiesReturnsFailureThenUnknownErrorIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device/", {"iaf.0"}});
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::string fabricId("0x10000000");
        memcpy(buf, fabricId.c_str(), fabricId.size());
        return fabricId.size();
    });

    setupExpectInitMultiple();
    setupExpectCleanupMultiple();

    MockNlApi *pMockNlApi = static_cast<MockNlApi *>(testIafNlApi.pNlApi.get());
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.clear();
    pMockNlApi->mockGenlRegisterFamilyReturnValue.clear();
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.clear();

    std::vector<L0::Sysman::IafPort> ports;

    // Override the registerfamily to return error
    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);

    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);

    pMockNlApi->mockGenlRegisterFamilyArgumentValue.push_back(pOps);
    pMockNlApi->mockGenlRegisterFamilyReturnValue.push_back(-6);
    pMockNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);

    ze_result_t result = testIafNlApi.getPorts("/sys/class/drm/card1/device/", ports);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    NEO::directoryFilesMap.clear();
}

} // namespace ult
} // namespace Sysman
} // namespace L0
