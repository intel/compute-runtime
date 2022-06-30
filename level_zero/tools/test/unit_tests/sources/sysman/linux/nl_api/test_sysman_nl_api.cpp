/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_nl_api.h"

extern bool sysmanUltsEnable;

using ::testing::Invoke;
using ::testing::Return;

namespace L0 {
namespace ult {

class PublicNlApi : public NlApi {
  public:
    using NlApi::genlLibraryHandle;
};

class SysmanNlApiFixture : public ::testing::Test {
  protected:
    PublicNlApi testNlApi;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        auto mockNlDll = std::make_unique<MockNlDll>();

        testNlApi.genlLibraryHandle = std::move(mockNlDll);
        EXPECT_TRUE(testNlApi.loadEntryPoints());
    }
    void TearDown() override {
    }
    bool testLoadEntryPointsWithMissingFunction(const std::string &procName) {
        PublicNlApi localNlApi;
        auto mockNlDll = std::make_unique<MockNlDll>();
        mockNlDll->deleteEntryPoint(procName);
        localNlApi.genlLibraryHandle = std::move(mockNlDll);

        return localNlApi.loadEntryPoints();
    }

  public:
    static const int testAttr;
};
const int SysmanNlApiFixture::testAttr = 1;

TEST_F(SysmanNlApiFixture, GivenNlApiWhenMissingDllEntryPointThenVerifyLoadEntryPointsFails) {
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("genl_connect"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("genl_ctrl_resolve"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("genl_handle_msg"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("genlmsg_put"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("genl_ops_resolve"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("genl_register_family"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("genl_unregister_family"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nl_recvmsgs_default"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nl_send_auto"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nl_socket_alloc"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nl_socket_disable_seq_check"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nl_socket_free"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nl_socket_modify_cb"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_data"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_get_u32"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_get_u64"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_get_u8"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_is_nested"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_len"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_next"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_ok"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_put_u16"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_put_u32"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_put_u64"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_put_u8"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nla_type"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nlmsg_alloc"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nlmsg_attrdata"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nlmsg_attrlen"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nlmsg_free"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("nlmsg_hdr"));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenMissingDllHandleThenVerifyLoadEntryPointsFails) {
    testNlApi.genlLibraryHandle.reset();
    EXPECT_FALSE(testNlApi.loadEntryPoints());
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyGenlConnectReturnsZero) {
    EXPECT_EQ(0, testNlApi.genlConnect(&MockNlDll::mockNlSock));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyGenlCtrlReturnsZero) {
    EXPECT_EQ(0, testNlApi.genlCtrlResolve(&MockNlDll::mockNlSock, const_cast<char *>(MockNlDll::mockFamilyName)));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyGenlHandleMsgReturnsZero) {
    EXPECT_EQ(0, testNlApi.genlHandleMsg(&MockNlDll::mockNlMsg, MockNlDll::mockArgP));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyGenlmsgPutReturnsValidPointer) {
    EXPECT_NE(nullptr, testNlApi.genlmsgPut(&MockNlDll::mockNlMsg, MockNlDll::mockPort, MockNlDll::mockSeq, MockNlDll::mockFamilyId, MockNlDll::mockHdrlen, MockNlDll::mockFlags, MockNlDll::mockCmd, MockNlDll::mockVersion));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyGenlOpsResolveReturnsZero) {
    EXPECT_EQ(0, testNlApi.genlOpsResolve(&MockNlDll::mockNlSock, &MockNlDll::mockGenlOps));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyGenlRegisterFamilyReturnsZero) {
    EXPECT_EQ(0, testNlApi.genlRegisterFamily(&MockNlDll::mockGenlOps));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyGenlUnregisterFamilyReturnsZero) {
    EXPECT_EQ(0, testNlApi.genlUnregisterFamily(&MockNlDll::mockGenlOps));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlSocketAllocReturnsValidPointer) {
    EXPECT_EQ(&MockNlDll::mockNlSock, testNlApi.nlSocketAlloc());
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlSocketDisableSeqCheckCompletesSuccessfully) {
    testNlApi.nlSocketDisableSeqCheck(&MockNlDll::mockNlSock);
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlSocketFreeCompletesSuccessfully) {
    testNlApi.nlSocketFree(&MockNlDll::mockNlSock);
}

TEST_F(SysmanNlApiFixture, GivenValidNlSockWhenCallingNlSocketModifyCbThenVerifyNlSocketModifyCbReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlSocketModifyCb(&MockNlDll::mockNlSock, MockNlDll::mockCbType, MockNlDll::mockCbKind, MockNlDll::mockCb, MockNlDll::mockArgP));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlRecvmsgsDefaultReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlRecvmsgsDefault(&MockNlDll::mockNlSock));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlSendAutoReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlSendAuto(&MockNlDll::mockNlSock, &MockNlDll::mockNlMsg));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaDataReturnsValidPointer) {
    EXPECT_NE(nullptr, testNlApi.nlaData(&MockNlDll::mockNlattr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaGetU32ReturnsValue) {
    EXPECT_EQ(MockNlDll::mockU32Val, testNlApi.nlaGetU32(&MockNlDll::mockNlattr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaGetU64ReturnsValue) {
    EXPECT_EQ(MockNlDll::mockU64Val, testNlApi.nlaGetU64(&MockNlDll::mockNlattr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaGetU8ReturnsValue) {
    EXPECT_EQ(MockNlDll::mockU8Val, testNlApi.nlaGetU8(&MockNlDll::mockNlattr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaIsNestedReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlaIsNested(&MockNlDll::mockNlattr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaLenReturnsValue) {
    EXPECT_EQ(MockNlDll::mockAttrLen, testNlApi.nlaLen(&MockNlDll::mockNlattr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaNextReturnsZero) {
    int remaining = MockNlDll::mockRemainBefore;

    EXPECT_EQ(&MockNlDll::mockNextNlattr, testNlApi.nlaNext(&MockNlDll::mockNlattr, &remaining));
    EXPECT_EQ(MockNlDll::mockRemainAfter, remaining);
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaOkReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlaOk(&MockNlDll::mockNlattr, MockNlDll::mockRemainBefore));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaPutU16ReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlaPutU16(&MockNlDll::mockNlMsg, MockNlDll::mockType, MockNlDll::mockU16Val));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaPutU32ReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlaPutU32(&MockNlDll::mockNlMsg, MockNlDll::mockType, MockNlDll::mockU32Val));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaPutU64ReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlaPutU64(&MockNlDll::mockNlMsg, MockNlDll::mockType, MockNlDll::mockU64Val));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaPutU8ReturnsZero) {
    EXPECT_EQ(0, testNlApi.nlaPutU8(&MockNlDll::mockNlMsg, MockNlDll::mockType, MockNlDll::mockU8Val));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlaTypeReturnsType) {
    EXPECT_EQ(MockNlDll::mockType, testNlApi.nlaType(&MockNlDll::mockNlattr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlmsgAllocReturnsNlMsg) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, testNlApi.nlmsgAlloc());
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlmsgFreeReturnsSuccessfully) {
    testNlApi.nlmsgFree(&MockNlDll::mockNlMsg);
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlmsgAttrdataReturnsNlattr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, testNlApi.nlmsgAttrdata(&MockNlDll::mockNlmsghdr, MockNlDll::mockAttr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlmsgAttrlenReturnsLength) {
    EXPECT_EQ(MockNlDll::mockAttrLen, testNlApi.nlmsgAttrlen(&MockNlDll::mockNlmsghdr, MockNlDll::mockAttr));
}

TEST_F(SysmanNlApiFixture, GivenNlApiWhenCompleteMockNlDllThenVerifyNlmsgHdrReturnsNlmsghdr) {
    EXPECT_EQ(&MockNlDll::mockNlmsghdr, testNlApi.nlmsgHdr(&MockNlDll::mockNlMsg));
}
} // namespace ult
} // namespace L0
