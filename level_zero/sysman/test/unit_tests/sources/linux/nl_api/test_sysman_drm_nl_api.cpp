/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_nl_api.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_sysman_drm_nl_api.h"

#include "gtest/gtest.h"

struct nl_sock {};

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanDrmNlApiFixture : public ::testing::Test {
  protected:
    std::unique_ptr<MockDrmNlApi> drmNlApi = nullptr;
    struct genl_ops ops = {};
    struct nl_sock nlSock = {};

    void SetUp() override {
        drmNlApi = std::make_unique<MockDrmNlApi>("testCard1");
    }

    void TearDown() override {
    }

    std::unique_ptr<MockNlApi> setupInitConnectionSuccess(bool enableRepeatedCalls = false) {
        auto pNlApi = std::make_unique<MockNlApi>();
        pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
        pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
        pNlApi->mockGenlConnectReturnValue.push_back(0);
        pNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
        pNlApi->mockGenlRegisterFamilyArgumentValue.push_back(&ops);
        pNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);
        pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
        pNlApi->mockNlSocketModifyCbReturnValue.push_back(0);
        if (enableRepeatedCalls) {
            pNlApi->isRepeated = true;
        }
        return pNlApi;
    }

    std::unique_ptr<MockNlApi> setupMockNlApiForReadSingleOperation(bool enableRepeatedCalls = false) {
        auto pNlApi = setupInitConnectionSuccess(enableRepeatedCalls);
        pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
        pNlApi->mockNlSendAutoReturnValue.push_back(1);
        pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
        pNlApi->readSingleError = true;
        pNlApi->isErrorCounterAvailable = true;
        return pNlApi;
    }

    std::unique_ptr<MockNlApi> setupMockNlApiForReadAllOperation(bool enableRepeatedCalls = false) {
        auto pNlApi = setupInitConnectionSuccess(enableRepeatedCalls);
        pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
        pNlApi->mockNlSendAutoReturnValue.push_back(1);
        pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
        pNlApi->queryErrorList = true;
        pNlApi->isErrorAvailable = true;
        return pNlApi;
    }

    std::unique_ptr<MockNlApi> setupMockNlApiForListNodesOperation(bool enableRepeatedCalls = false) {
        auto pNlApi = setupInitConnectionSuccess(enableRepeatedCalls);
        pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
        pNlApi->mockNlSendAutoReturnValue.push_back(1);
        pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
        pNlApi->queryNodeList = true;
        return pNlApi;
    }

    std::unique_ptr<MockNlApi> setupMockNlApiForClearOperation(bool errorDataInvalid = false) {
        auto pNlApi = setupInitConnectionSuccess();
        pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
        pNlApi->mockNlSendAutoReturnValue.push_back(1);
        pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
        pNlApi->clearErrorCounter = true;
        pNlApi->isErrorDataInvalid = errorDataInvalid;
        return pNlApi;
    }
};

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketAllocFailsWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlConnectFailsWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pNlApi->mockGenlConnectReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlRegisterFamilyReturnsExistWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockGenlRegisterFamilyReturnValue.push_back(-6);
    pNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_NOT_READY, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlRegisterFamilyReturnsOtherErrorWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockGenlRegisterFamilyReturnValue.push_back(-1);
    pNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(true);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlOpsResolveFailsWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pNlApi->mockGenlOpsResolveReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlCtrlResolveFailsWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pNlApi->mockGenlOpsResolveReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketModifyCbFailsWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pNlApi->mockGenlOpsResolveReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
    pNlApi->mockNlSocketModifyCbReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketModifyCbFailsForHandleAckWhenCallingInitConnectionThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    pNlApi->isMockGenlRegisterFamilyRepeatedCall.push_back(false);
    pNlApi->mockGenlOpsResolveReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
    pNlApi->mockNlSocketModifyCbReturnValue.push_back(0);
    pNlApi->mockNlSocketModifyCbReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingInitConnectionThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupInitConnectionSuccess();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenInitConnectionIsCalledTwiceThenSecondCallReturnsSuccess) {
    drmNlApi->pNlApi = setupInitConnectionSuccess(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->initConnection());
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->initConnection());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndAllocMsgSucceedsWhenCallingGetErrorCounterThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForReadSingleOperation();
    DrmErrorCounter errorCounter = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->getErrorCounter(0, 0, errorCounter));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndErrorsNotAvailableWhenCallingGetErrorCounterThenSuccessIsReturned) {
    auto pNlApi = setupMockNlApiForReadSingleOperation();
    pNlApi->isErrorCounterAvailable = false;
    drmNlApi->pNlApi = std::move(pNlApi);
    DrmErrorCounter errorCounter = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->getErrorCounter(0, 0, errorCounter));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingGetErrorCountersThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    DrmErrorCounter errorCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->getErrorCounter(0, 0, errorCounter));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndAllocMsgFailsWhenCallingGetErrorCountersThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    DrmErrorCounter errorCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->getErrorCounter(0, 0, errorCounter));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlmsgPutFailsWhenCallingGetErrorCounterThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess(true);
    pNlApi->mockGenlmsgPutReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    DrmErrorCounter errorCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->getErrorCounter(0, 0, errorCounter));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingCallingGetErrorsListThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndAllocMsgFailsWhenCallingGetErrorsListThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlMsgPutFailsWhenCallingGetErrorsListThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess(true);
    pNlApi->mockGenlmsgPutReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSendAutoFailsWhenCallingGetErrorsListThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
    pNlApi->mockNlSendAutoReturnValue.push_back(-1);
    pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
    drmNlApi->pNlApi = std::move(pNlApi);
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlRecvmsgsDefaultFailsWithPermissionErrorWhenCallingGetErrorsListThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
    pNlApi->mockNlSendAutoReturnValue.push_back(1);
    pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(-NLE_PERM);
    drmNlApi->pNlApi = std::move(pNlApi);
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlRecvmsgsDefaultFailsWithNonPermErrorWhenCallingGetErrorsListThenUnknownErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
    pNlApi->mockNlSendAutoReturnValue.push_back(1);
    pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(-NLE_FAILURE);
    drmNlApi->pNlApi = std::move(pNlApi);
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingGetErrorsListThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForReadAllOperation();
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndErrorListNotAvailableWhenCallingGetErrorsListThenSuccessIsReturned) {
    auto pNlApi = setupMockNlApiForReadAllOperation();
    pNlApi->isErrorAvailable = false;
    drmNlApi->pNlApi = std::move(pNlApi);
    std::vector<DrmErrorCounter> errorList = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->getErrorsList(0, errorList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingListNodesThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->callRealListNodes = true;
    std::vector<DrmRasNode> nodeList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->listNodes(nodeList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndAllocMsgFailsWhenCallingListNodesThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->callRealListNodes = true;
    std::vector<DrmRasNode> nodeList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->listNodes(nodeList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlmsgPutFailsWhenCallingListNodesThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess(true);
    pNlApi->mockGenlmsgPutReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->callRealListNodes = true;
    std::vector<DrmRasNode> nodeList = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->listNodes(nodeList));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingListNodesThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForListNodesOperation();
    drmNlApi->callRealListNodes = true;
    std::vector<DrmRasNode> nodeList = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->listNodes(nodeList));
    EXPECT_EQ(1u, nodeList.size());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndKernelReturnsErrorWhenCallingClearErrorCounterThenErrorIsReturned) {
    bool isKernelDataInvalid = true;
    drmNlApi->pNlApi = setupMockNlApiForClearOperation(isKernelDataInvalid);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, drmNlApi->clearErrorCounter(0, 0));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingClearErrorCounterThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForClearOperation();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->clearErrorCounter(0, 1));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndAllocMsgFailsWhenCallingClearErrorCounterThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->clearErrorCounter(0, 0));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingClearErrorCounterThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->clearErrorCounter(0, 0));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithGetErrorCounterOperationWhenCallingHandleAckThenOperationStateRemainsUnchangedAndSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EINVAL;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_GET_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();

    int result = drmNlApi->handleAck(msg);

    EXPECT_FALSE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithNullCurrentOperationWhenCallingHandleAckThenNoOperationIsPerformedAndNlStopIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();

    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EINVAL;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = nullptr;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_EQ(NL_STOP, result);
    EXPECT_EQ(nullptr, drmNlApi->currentOperation);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithNonErrorMessageTypeWhenCallingHandleAckThenNlStopIsReturned) {

    static struct {
        struct nlmsghdr hdr;
    } customHeader;

    customHeader.hdr.nlmsg_type = NLMSG_DONE;
    customHeader.hdr.nlmsg_len = NLMSG_LENGTH(0);

    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = &customHeader.hdr;

    drmNlApi->pNlApi = std::move(pNlApi);

    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();

    int result = drmNlApi->handleAck(msg);

    EXPECT_FALSE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithMalformedErrorMessageWhenCallingHandleAckThenOperationReturnsErrorAndNlStopIsReturned) {

    static struct {
        struct nlmsghdr hdr;
    } malformedHeader;

    malformedHeader.hdr.nlmsg_type = NLMSG_ERROR;

    // Set length to only include header, not nlmsgerr payload
    malformedHeader.hdr.nlmsg_len = NLMSG_LENGTH(0);

    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = &malformedHeader.hdr;

    drmNlApi->pNlApi = std::move(pNlApi);

    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithNullHeaderWhenCallingHandleAckForClearOperationThenOperationReturnsError) {

    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = nullptr;

    drmNlApi->pNlApi = std::move(pNlApi);

    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsEPERMErrorOnClearErrorCounterThenInsufficientPermissionsErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EPERM;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsEACCESErrorOnClearErrorCounterThenInsufficientPermissionsErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EACCES;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsENOENTErrorOnClearErrorCounterThenInvalidArgumentErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -ENOENT;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsENODEVErrorOnClearErrorCounterThenDeviceLostErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -ENODEV;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsEINVALErrorOnClearErrorCounterThenInvalidArgumentErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EINVAL;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsEOPNOTSUPPErrorOnClearErrorCounterThenUnsupportedFeatureErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EOPNOTSUPP;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsENOSYSErrorOnClearErrorCounterThenUnsupportedFeatureErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -ENOSYS;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsEBUSYErrorOnClearErrorCounterThenNotReadyErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EBUSY;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_NOT_READY, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsUnknownErrorOnClearErrorCounterThenUnknownErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = -EAGAIN;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenKernelReturnsSuccessOnClearErrorCounterThenSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = NLMSG_ERROR;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
    pNlApi->mockNlmsghdr.err.error = 0;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_ERROR_UNKNOWN;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlmsgHdrReturnsNullWhenCallingHandleAckThenNlStopAndSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = nullptr;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_CLEAR_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_TRUE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithNullHeaderAndGetErrorCounterOperationWhenCallingHandleAckThenOperationStateRemainsUnchanged) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = nullptr;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_GET_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_FALSE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithNullHeaderAndNullCurrentOperationWhenCallingHandleAckThenNlStopIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = nullptr;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = nullptr;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_EQ(NL_STOP, result);
    EXPECT_EQ(nullptr, drmNlApi->currentOperation);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithMalformedErrorMessageAndGetErrorCounterOperationWhenCallingHandleAckThenOperationStateRemainsUnchanged) {
    static struct {
        struct nlmsghdr hdr;
    } malformedHeader;

    malformedHeader.hdr.nlmsg_type = NLMSG_ERROR;
    malformedHeader.hdr.nlmsg_len = NLMSG_LENGTH(0);

    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = &malformedHeader.hdr;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_GET_ERROR_COUNTER, nullptr);
    drmNlApi->currentOperation->done = false;
    drmNlApi->currentOperation->result = ZE_RESULT_SUCCESS;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_FALSE(drmNlApi->currentOperation->done);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->currentOperation->result);
    EXPECT_EQ(NL_STOP, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWithMalformedErrorMessageAndNullCurrentOperationWhenCallingHandleAckThenNlStopIsReturned) {
    static struct {
        struct nlmsghdr hdr;
    } malformedHeader;

    malformedHeader.hdr.nlmsg_type = NLMSG_ERROR;
    malformedHeader.hdr.nlmsg_len = NLMSG_LENGTH(0); // Too small

    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = &malformedHeader.hdr;

    drmNlApi->pNlApi = std::move(pNlApi);
    drmNlApi->currentOperation = nullptr;

    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleAck(msg);

    EXPECT_EQ(NL_STOP, result);
    EXPECT_EQ(nullptr, drmNlApi->currentOperation);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
