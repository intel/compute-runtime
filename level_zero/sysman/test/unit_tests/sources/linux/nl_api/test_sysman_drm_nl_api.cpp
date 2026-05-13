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
    struct nl_sock nlEventSock = {}; // Separate socket for event subscriptions

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

    std::unique_ptr<MockNlApi> setupMockNlApiForEventSubscription(bool enableRepeatedCalls = false) {
        auto pNlApi = std::make_unique<MockNlApi>();
        pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
        pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
        pNlApi->eventSocketPtr = &nlEventSock;
        pNlApi->mockGenlConnectReturnValue.push_back(0);
        pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
        pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
        pNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(100);
        pNlApi->mockNlSocketAddMembershipReturnValue.push_back(0);
        pNlApi->mockNlSocketModifyCbReturnValue.push_back(0);
        if (enableRepeatedCalls) {
            pNlApi->isRepeated = true;
        }
        return pNlApi;
    }

    std::unique_ptr<MockNlApi> setupMockNlApiForSetThresholdOperation(bool errorDataInvalid = false) {
        auto pNlApi = setupInitConnectionSuccess();
        pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
        pNlApi->mockNlSendAutoReturnValue.push_back(1);
        pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
        pNlApi->setThreshold = true;
        pNlApi->isErrorDataInvalid = errorDataInvalid;
        return pNlApi;
    }

    std::unique_ptr<MockNlApi> setupMockNlApiForGetThresholdOperation() {
        auto pNlApi = setupInitConnectionSuccess();
        pNlApi->mockGenlmsgPutReturnValue.push_back(pNlApi.get());
        pNlApi->mockNlSendAutoReturnValue.push_back(1);
        pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
        pNlApi->getThreshold = true;
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

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingSubscribeToEventsSuccessfullyThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_TRUE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingSubscribeToEventsThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketAllocFailsWhenCallingSubscribeToEventsThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlConnectFailsWhenCallingSubscribeToEventsThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketSetNonblockingFailsWhenCallingSubscribeToEventsThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlCtrlResolveFailsWhenCallingSubscribeToEventsThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndGenlCtrlResolveGrpFailsWhenCallingSubscribeToEventsThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
    pNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketAddMembershipFailsWithPermissionErrorWhenCallingSubscribeToEventsThenInsufficientPermissionsIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
    pNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(100);
    pNlApi->mockNlSocketAddMembershipReturnValue.push_back(-NLE_PERM);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketAddMembershipFailsWithOtherErrorWhenCallingSubscribeToEventsThenUnknownErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
    pNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(100);
    pNlApi->mockNlSocketAddMembershipReturnValue.push_back(-NLE_FAILURE);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndNlSocketModifyCbFailsWhenCallingSubscribeToEventsThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
    pNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(100);
    pNlApi->mockNlSocketAddMembershipReturnValue.push_back(0);
    pNlApi->mockNlSocketModifyCbReturnValue.push_back(-1);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingUnsubscribeFromEventsWithoutSubscriptionThenSuccessIsReturned) {
    drmNlApi->pNlApi = std::make_unique<MockNlApi>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingUnsubscribeFromEventsAfterSubscriptionThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_TRUE(drmNlApi->isSubscribedToEvents());
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingUnsubscribeFromEventsTwiceThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingIsSubscribedToEventsBeforeSubscriptionThenFalseIsReturned) {
    drmNlApi->pNlApi = std::make_unique<MockNlApi>();
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingIsSubscribedToEventsAfterSubscriptionThenTrueIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_TRUE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingIsSubscribedToEventsAfterUnsubscriptionThenFalseIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_TRUE(drmNlApi->isSubscribedToEvents());
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingPollEventWithNoEventReadyThenNotReadyIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
    pNlApi->receiveEventData = false; // No event data

    DrmRasEvent event = {};
    EXPECT_EQ(ZE_RESULT_NOT_READY, drmNlApi->pollEvent(event));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingPollEventWithEventReadyThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(0);
    pNlApi->receiveEventData = true;
    pNlApi->eventNodeId = 123;
    pNlApi->eventErrorId = 456;

    DrmRasEvent event = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->pollEvent(event));
    EXPECT_EQ(123u, event.nodeId);
    EXPECT_EQ(456u, event.errorId);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingPollEventAndNlRecvmsgsDefaultFailsWithPermissionErrorThenInsufficientPermissionsIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(-NLE_PERM);

    DrmRasEvent event = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, drmNlApi->pollEvent(event));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingPollEventAndNlRecvmsgsDefaultFailsWithOtherErrorThenUnknownErrorIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlRecvmsgsDefaultReturnValue.push_back(-NLE_FAILURE);

    DrmRasEvent event = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->pollEvent(event));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingPollEventWithoutSubscriptionThenUninitializedErrorIsReturned) {
    drmNlApi->pNlApi = std::make_unique<MockNlApi>();

    DrmRasEvent event = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, drmNlApi->pollEvent(event));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingGetEventSocketFdWithoutSubscriptionThenNegativeOneIsReturned) {
    drmNlApi->pNlApi = std::make_unique<MockNlApi>();
    EXPECT_EQ(-1, drmNlApi->getEventSocketFd());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingGetEventSocketFdAfterSubscriptionThenValidFdIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlSocketGetFdReturnValue.push_back(42);

    int fd = drmNlApi->getEventSocketFd();
    EXPECT_EQ(42, fd);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingGetEventSocketFdAfterUnsubscriptionThenNegativeOneIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_EQ(-1, drmNlApi->getEventSocketFd());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingHandleEventMsgWithValidMessageThenEventIsProcessedAndOkIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->receiveEventData = true;
    pNlApi->eventNodeId = 789;
    pNlApi->eventErrorId = 321;

    struct nl_msg *msg = pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleEventMsg(msg);

    EXPECT_EQ(NL_OK, result);
    pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingHandleEventMsgWithNullHeaderThenStopIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->nlmsgHdrResult = nullptr;
    struct nl_msg *msg = pNlApi->nlmsgAlloc();
    int result = drmNlApi->handleEventMsg(msg);
    EXPECT_EQ(NL_STOP, result);
    pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenHandleEventMsgWithParseFailureThenOkIsReturnedAndEventReadyRemainsUnset) {
    drmNlApi->pNlApi = std::make_unique<MockNlApi>();

    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->nlmsgHdrResult = &pNlApi->mockNlmsghdr.hdr;
    pNlApi->mockNlmsghdr.hdr.nlmsg_type = 0;
    pNlApi->mockNlmsghdr.hdr.nlmsg_len = NLMSG_LENGTH(0);
    pNlApi->receiveEventData = false;

    struct nl_msg *msg = pNlApi->nlmsgAlloc();
    DrmRasEvent event = {};

    drmNlApi->eventReady = false;
    pNlApi->nlmsgHdrResult = nullptr;
    ze_result_t parseResult = drmNlApi->parseEventMessage(msg, event);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, parseResult);
    EXPECT_FALSE(drmNlApi->eventReady);

    pNlApi->nlmsgHdrResult = &pNlApi->mockNlmsghdr.hdr;
    drmNlApi->eventReady = false;

    pNlApi->receiveEventData = false;
    int result = drmNlApi->handleEventMsg(msg);
    EXPECT_EQ(NL_OK, result);

    pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenSubscribingToEventsSuccessfullyThenNlSocketDisableSeqCheckIsCalled) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    EXPECT_EQ(0u, pNlApi->nlSocketDisableSeqCheckCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_EQ(1u, pNlApi->nlSocketDisableSeqCheckCalled);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenUnsubscribingFromEventsThenCleanupOperationsArePerformed) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlSocketDropMembershipReturnValue.push_back(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    uint32_t initialSocketFreeCalls = pNlApi->nlSocketFreeCalled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_GT(pNlApi->nlSocketFreeCalled, initialSocketFreeCalls);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCleanupEventSocketIsCalledThenStateIsProperlyReset) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlSocketDropMembershipReturnValue.push_back(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_TRUE(drmNlApi->isSubscribedToEvents());
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
    EXPECT_EQ(-1, drmNlApi->getEventSocketFd());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenInitEventSocketFailsAtNlSocketAllocThenCleanupDoesNotCrash) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingGetEventSocketFdWithNullSocketButSubscribedThenNegativeOneIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());
    EXPECT_TRUE(drmNlApi->isSubscribedToEvents());
    drmNlApi->nlEventSock = nullptr;
    EXPECT_EQ(-1, drmNlApi->getEventSocketFd());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenEventSubscribedIsFalseAndGetEventSocketFdIsCalledThenNegativeOneIsReturned) {
    drmNlApi->pNlApi = std::make_unique<MockNlApi>();
    drmNlApi->nlEventSock = &nlEventSock;
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
    EXPECT_EQ(-1, drmNlApi->getEventSocketFd());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenInitEventSocketFailsAfterAllocatingSocketThenSocketIsProperlyFreed) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(-1); // Fail at genlConnect
    drmNlApi->pNlApi = std::move(pNlApi);

    uint32_t initialSocketFreeCalls = static_cast<MockNlApi *>(drmNlApi->pNlApi.get())->nlSocketFreeCalled;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
    EXPECT_GT(static_cast<MockNlApi *>(drmNlApi->pNlApi.get())->nlSocketFreeCalled, initialSocketFreeCalls);
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenInitEventSocketFailsAfterAddingMembershipThenMembershipIsProperlyDropped) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    pNlApi->mockNlSocketAllocReturnValue.push_back(&nlEventSock);
    pNlApi->mockGenlConnectReturnValue.push_back(0);
    pNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
    pNlApi->mockGenCtrlResolveReturnValue.push_back(1);
    pNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(100);
    pNlApi->mockNlSocketAddMembershipReturnValue.push_back(0);
    pNlApi->mockNlSocketModifyCbReturnValue.push_back(-1); // Fail at nlSocketModifyCb
    pNlApi->mockNlSocketDropMembershipReturnValue.push_back(0);
    drmNlApi->pNlApi = std::move(pNlApi);
    auto mockNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    uint32_t initialSocketFreeCalls = mockNlApi->nlSocketFreeCalled;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->subscribeToEvents());
    EXPECT_GT(mockNlApi->nlSocketFreeCalled, initialSocketFreeCalls);
    EXPECT_FALSE(drmNlApi->isSubscribedToEvents());
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCleanupEventSocketIsCalledMultipleTimesThenItHandlesGracefully) {
    drmNlApi->pNlApi = setupMockNlApiForEventSubscription();
    auto pNlApi = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApi->mockNlSocketDropMembershipReturnValue.push_back(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->subscribeToEvents());

    // First cleanup
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    uint32_t firstUnsubscribeSocketFreeCalls = pNlApi->nlSocketFreeCalled;

    // Second cleanup should be safe (no crash) and not attempt to free again
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->unsubscribeFromEvents());
    EXPECT_EQ(firstUnsubscribeSocketFreeCalls, pNlApi->nlSocketFreeCalled);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithBothNodeIdAndErrorIdThenSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->receiveEventData = true;
    pNlApi->eventNodeId = 42;
    pNlApi->eventErrorId = 123;
    pNlApi->processingEventSocket = true;
    drmNlApi->pNlApi = std::move(pNlApi);

    DrmRasEvent event = {};
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    ze_result_t result = drmNlApi->parseEventMessage(msg, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(42u, event.nodeId);
    EXPECT_EQ(123u, event.errorId);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithOnlyNodeIdThenSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->receiveEventData = true;
    pNlApi->eventNodeId = 99;
    pNlApi->eventErrorId = 0;
    pNlApi->processingEventSocket = true;
    drmNlApi->pNlApi = std::move(pNlApi);

    DrmRasEvent event = {999, 888}; // Initialize with non-zero values to verify they are reset
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();

    // Manually create attribute chain with only NODE_ID
    auto pNlApiPtr = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    if (pNlApiPtr->eventAttrChain != nullptr) {
        delete pNlApiPtr->eventAttrChain;
    }
    pNlApiPtr->eventAttrChain = new MyNlattr;
    pNlApiPtr->eventAttrChain->type = DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID;
    pNlApiPtr->eventAttrChain->content = 99;
    pNlApiPtr->eventAttrChain->next = nullptr;
    ze_result_t result = drmNlApi->parseEventMessage(msg, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(99u, event.nodeId);
    EXPECT_EQ(0u, event.errorId); // Should be reset to 0

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithOnlyErrorIdThenSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->receiveEventData = true;
    pNlApi->eventNodeId = 0;
    pNlApi->eventErrorId = 456;
    pNlApi->processingEventSocket = true;
    drmNlApi->pNlApi = std::move(pNlApi);

    DrmRasEvent event = {777, 666}; // Initialize with non-zero values to verify they are reset
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();

    // Manually create attribute chain with only ERROR_ID
    auto pNlApiPtr = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    if (pNlApiPtr->eventAttrChain != nullptr) {
        delete pNlApiPtr->eventAttrChain;
    }
    pNlApiPtr->eventAttrChain = new MyNlattr;
    pNlApiPtr->eventAttrChain->type = DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_ID;
    pNlApiPtr->eventAttrChain->content = 456;
    pNlApiPtr->eventAttrChain->next = nullptr;
    ze_result_t result = drmNlApi->parseEventMessage(msg, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, event.nodeId); // Should be reset to 0
    EXPECT_EQ(456u, event.errorId);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithNoAttributesThenSuccessIsReturnedAndEventIsReset) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->receiveEventData = false; // No event data
    pNlApi->processingEventSocket = true;
    drmNlApi->pNlApi = std::move(pNlApi);

    DrmRasEvent event = {555, 444}; // Initialize with non-zero values to verify they are reset
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    ze_result_t result = drmNlApi->parseEventMessage(msg, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, event.nodeId);  // Should be reset to 0
    EXPECT_EQ(0u, event.errorId); // Should be reset to 0

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithNullHeaderThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->nlmsgHdrResult = nullptr; // Simulate null header
    drmNlApi->pNlApi = std::move(pNlApi);

    DrmRasEvent event = {};
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    ze_result_t result = drmNlApi->parseEventMessage(msg, event);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithMaximumValuesThenSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->receiveEventData = true;
    pNlApi->eventNodeId = UINT32_MAX;
    pNlApi->eventErrorId = UINT32_MAX;
    pNlApi->processingEventSocket = true;
    drmNlApi->pNlApi = std::move(pNlApi);

    DrmRasEvent event = {};
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();
    ze_result_t result = drmNlApi->parseEventMessage(msg, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(UINT32_MAX, event.nodeId);
    EXPECT_EQ(UINT32_MAX, event.errorId);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageCalledMultipleTimesThenEachCallResetsEventCorrectly) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->processingEventSocket = true;
    drmNlApi->pNlApi = std::move(pNlApi);

    // First call with specific values
    auto pNlApiPtr = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());
    pNlApiPtr->receiveEventData = true;
    pNlApiPtr->eventNodeId = 10;
    pNlApiPtr->eventErrorId = 20;

    DrmRasEvent event = {};
    struct nl_msg *msg1 = drmNlApi->pNlApi->nlmsgAlloc();
    ze_result_t result1 = drmNlApi->parseEventMessage(msg1, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result1);
    EXPECT_EQ(10u, event.nodeId);
    EXPECT_EQ(20u, event.errorId);
    drmNlApi->pNlApi->nlmsgFree(msg1);

    // Second call with different values - event should be reset first
    if (pNlApiPtr->eventAttrChain != nullptr) {
        delete pNlApiPtr->eventAttrChain;
        pNlApiPtr->eventAttrChain = nullptr;
    }
    pNlApiPtr->eventNodeId = 30;
    pNlApiPtr->eventErrorId = 40;

    struct nl_msg *msg2 = drmNlApi->pNlApi->nlmsgAlloc();
    ze_result_t result2 = drmNlApi->parseEventMessage(msg2, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result2);
    EXPECT_EQ(30u, event.nodeId);
    EXPECT_EQ(40u, event.errorId);
    drmNlApi->pNlApi->nlmsgFree(msg2);

    // Third call with no data - event should be reset to zeros
    if (pNlApiPtr->eventAttrChain != nullptr) {
        delete pNlApiPtr->eventAttrChain;
        pNlApiPtr->eventAttrChain = nullptr;
    }
    pNlApiPtr->receiveEventData = false;

    struct nl_msg *msg3 = drmNlApi->pNlApi->nlmsgAlloc();
    ze_result_t result3 = drmNlApi->parseEventMessage(msg3, event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result3);
    EXPECT_EQ(0u, event.nodeId);
    EXPECT_EQ(0u, event.errorId);
    drmNlApi->pNlApi->nlmsgFree(msg3);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithUnknownAttributeTypesThenTheyAreIgnoredAndSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->processingEventSocket = true;
    pNlApi->receiveEventData = true; // Enable event data processing
    drmNlApi->pNlApi = std::move(pNlApi);

    auto pNlApiPtr = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());

    // Create attribute chain with NODE_ID, unknown type, ERROR_ID, another unknown type
    // Must be created BEFORE parseEventMessage to prevent auto-creation
    MyNlattr *chain = new MyNlattr;
    MyNlattr *current = chain;

    // First: NODE_ID
    current->type = DRM_RAS_A_ERROR_COUNTER_ATTRS_NODE_ID;
    current->content = 100;
    current->next = new MyNlattr;
    current = current->next;

    // Second: Unknown attribute type (should be ignored)
    current->type = 9999; // Unknown type
    current->content = 12345;
    current->next = new MyNlattr;
    current = current->next;

    // Third: ERROR_ID
    current->type = DRM_RAS_A_ERROR_COUNTER_ATTRS_ERROR_ID;
    current->content = 200;
    current->next = new MyNlattr;
    current = current->next;

    // Fourth: Another unknown attribute type (should be ignored)
    current->type = 8888; // Unknown type
    current->content = 67890;
    current->next = nullptr;

    // Set the custom chain in the mock (this prevents auto-creation in nlmsgAttrdata)
    pNlApiPtr->eventAttrChain = chain;

    DrmRasEvent event = {};
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();

    ze_result_t result = drmNlApi->parseEventMessage(msg, event);

    // Unknown attributes should be silently ignored, only NODE_ID and ERROR_ID should be set
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(100u, event.nodeId);
    EXPECT_EQ(200u, event.errorId);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenParseEventMessageWithOnlyUnknownAttributesThenEventIsResetAndSuccessIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->processingEventSocket = true;
    pNlApi->receiveEventData = true; // Enable event data processing
    drmNlApi->pNlApi = std::move(pNlApi);

    auto pNlApiPtr = static_cast<MockNlApi *>(drmNlApi->pNlApi.get());

    // Create attribute chain with only unknown attribute types
    MyNlattr *chain = new MyNlattr;
    chain->type = 7777; // Unknown type
    chain->content = 11111;
    chain->next = new MyNlattr;
    chain->next->type = 6666; // Another unknown type
    chain->next->content = 22222;
    chain->next->next = nullptr;

    // Set the custom chain in the mock
    pNlApiPtr->eventAttrChain = chain;

    DrmRasEvent event = {555, 444}; // Initialize with non-zero values
    struct nl_msg *msg = drmNlApi->pNlApi->nlmsgAlloc();

    ze_result_t result = drmNlApi->parseEventMessage(msg, event);

    // All attributes are unknown and should be ignored, event should be reset to zeros
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, event.nodeId);
    EXPECT_EQ(0u, event.errorId);

    drmNlApi->pNlApi->nlmsgFree(msg);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenSubscribeToEventsCalledTwiceAfterUnsubscribeThenInittedRemainsTrue) {

    drmNlApi->pNlApi = setupMockNlApiForEventSubscription(true);
    ze_result_t result = drmNlApi->subscribeToEvents();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(drmNlApi->isInitDone);

    result = drmNlApi->unsubscribeFromEvents();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(drmNlApi->isInitDone);

    result = drmNlApi->subscribeToEvents();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(drmNlApi->isInitDone);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingSetErrorThresholdThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForSetThresholdOperation();
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->DrmNlApi::setErrorThreshold(0, 1, 500));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingSetErrorThresholdThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->DrmNlApi::setErrorThreshold(0, 1, 500));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndAllocMsgFailsWhenCallingSetErrorThresholdThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->DrmNlApi::setErrorThreshold(0, 1, 500));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceWhenCallingGetErrorThresholdThenSuccessIsReturned) {
    drmNlApi->pNlApi = setupMockNlApiForGetThresholdOperation();
    DrmErrorThreshold threshold = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, drmNlApi->DrmNlApi::getErrorThreshold(0, 1, threshold));
    EXPECT_EQ(100u, threshold.threshold);
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndLoadEntryPointsFailsWhenCallingGetErrorThresholdThenErrorIsReturned) {
    auto pNlApi = std::make_unique<MockNlApi>();
    pNlApi->mockLoadEntryPointsReturnValue.push_back(false);
    drmNlApi->pNlApi = std::move(pNlApi);
    DrmErrorThreshold threshold = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, drmNlApi->DrmNlApi::getErrorThreshold(0, 1, threshold));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiInstanceAndAllocMsgFailsWhenCallingGetErrorThresholdThenErrorIsReturned) {
    auto pNlApi = setupInitConnectionSuccess();
    pNlApi->mockNlmsgAllocReturnValue.push_back(nullptr);
    drmNlApi->pNlApi = std::move(pNlApi);
    DrmErrorThreshold threshold = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, drmNlApi->DrmNlApi::getErrorThreshold(0, 1, threshold));
}

TEST_F(SysmanDrmNlApiFixture, GivenDrmNlApiAndNoThresholdAttrsWhenCallingGetErrorThresholdRspThenThresholdRemainsZero) {
    DrmErrorThreshold threshold = {};
    drmNlApi->currentOperation = std::make_unique<MockDrmNlApi::Operation>(DRM_RAS_CMD_GET_ERROR_THRESHOLD, &threshold);

    auto pNlApi = std::make_unique<MockNlApi>();
    drmNlApi->pNlApi = std::move(pNlApi);

    auto attrs = std::make_unique<struct nlattr *[]>(DRM_RAS_A_ERROR_THRESHOLD_ATTRS_MAX + 1);
    std::fill(attrs.get(), attrs.get() + DRM_RAS_A_ERROR_THRESHOLD_ATTRS_MAX + 1, nullptr);

    struct genl_info info = {};
    info.attrs = attrs.get();
    info.nlh = nullptr;

    drmNlApi->getErrorThresholdRsp(nullptr, nullptr, &info);

    EXPECT_EQ(0u, threshold.errorId);
    EXPECT_EQ(0u, threshold.threshold);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
