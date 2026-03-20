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

} // namespace ult
} // namespace Sysman
} // namespace L0
