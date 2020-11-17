/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/linux/nl_api/nl_api.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

class SysmanNlApiFixture : public ::testing::Test {
  protected:
    NlApi *pNlApi = nullptr;
    struct nl_sock *pNlSock;

    void SetUp() override {
        pNlApi = NlApi::create();

        if (nullptr == pNlApi) {
            GTEST_SKIP();
        }
        pNlSock = pNlApi->nlSocketAlloc();
        EXPECT_NE(pNlSock, nullptr);
    }
    void TearDown() override {
        if (nullptr != pNlSock) {
            pNlApi->nlSocketFree(pNlSock);
            pNlSock = nullptr;
        }
        if (nullptr != pNlApi) {
            delete pNlApi;
            pNlApi = nullptr;
        }
    }

  public:
    static const char *testName;
    static const int testFamilyId;
    static const char *nlctrlName;
    static const int nlctrlFamilyId;
    static const int testAttrU8;
    static const int testAttrU16;
    static const int testAttrU32;
    static const int testAttrU64;
    static const int testAttrMax;
    static const int testCmd;
    static const char *testCmdName;
};
const char *SysmanNlApiFixture::testName = "test_family";
const int SysmanNlApiFixture::testFamilyId = 0x2000;
const char *SysmanNlApiFixture::nlctrlName = "nlctrl";
const int SysmanNlApiFixture::nlctrlFamilyId = 0x10; // Well known number
const int SysmanNlApiFixture::testAttrU8 = 1;
const int SysmanNlApiFixture::testAttrU16 = 2;
const int SysmanNlApiFixture::testAttrU32 = 3;
const int SysmanNlApiFixture::testAttrU64 = 4;
const int SysmanNlApiFixture::testAttrMax = SysmanNlApiFixture::testAttrU64;
const int SysmanNlApiFixture::testCmd = 1;
const char *SysmanNlApiFixture::testCmdName = "test_command";

TEST_F(SysmanNlApiFixture, GivenValidNlSockWhenCallingGenlConnectVerifyGenlConnectReturnsSuccess) {
    EXPECT_EQ(pNlApi->genlConnect(pNlSock), 0);
}
TEST_F(SysmanNlApiFixture, GivenValidNlSockWhenCallingGenlCtrlResolveVerifyGenlCtrlResolveReturnsSuccess) {
    EXPECT_EQ(pNlApi->genlConnect(pNlSock), 0);
    EXPECT_EQ(pNlApi->genlCtrlResolve(pNlSock, SysmanNlApiFixture::nlctrlName), SysmanNlApiFixture::nlctrlFamilyId);
}
TEST_F(SysmanNlApiFixture, GivenValidNlSockWhenCallingGenlOpsResolveVerifyGenlOpsResolveReturnsSuccess) {
    EXPECT_EQ(pNlApi->genlConnect(pNlSock), 0);
    struct genl_ops ops = {};
    ops.o_name = new char[strlen(SysmanNlApiFixture::nlctrlName + 1)];
    strcpy(ops.o_name, SysmanNlApiFixture::nlctrlName);
    EXPECT_EQ(pNlApi->genlOpsResolve(pNlSock, &ops), 0);
    delete ops.o_name;
}
TEST_F(SysmanNlApiFixture, GivenValidGenlOpsWhenCallingGenlRegisterFamilyVerifyGenlRegisterFamilyReturnsSuccess) {
    EXPECT_EQ(pNlApi->genlConnect(pNlSock), 0);
    struct genl_ops ops = {};
    ops.o_name = new char[strlen(SysmanNlApiFixture::testName) + 1];
    strcpy(ops.o_name, SysmanNlApiFixture::testName);
    EXPECT_EQ(pNlApi->genlRegisterFamily(&ops), 0);
    EXPECT_EQ(pNlApi->genlUnregisterFamily(&ops), 0);
    delete ops.o_name;
}
TEST_F(SysmanNlApiFixture, GivenValidNlApiWhenCallingNlmsgAllocVerifyNlmsgAllocReturnsValidMessage) {
    struct nl_msg *pNlMsg = pNlApi->nlmsgAlloc();
    EXPECT_NE(pNlMsg, nullptr);
    pNlApi->nlmsgFree(pNlMsg);
}
TEST_F(SysmanNlApiFixture, GivenValidNlSockWhenCallingNlSocketDisableSeqCheckVerifyNSocketDisableSeqCheckReturns) {
    // Void function, just verify that linkage works
    pNlApi->nlSocketDisableSeqCheck(pNlSock);
}
extern "C" {
static int callback(struct nl_msg *msg, void *arg) {
    NlApi *pNlApi = reinterpret_cast<NlApi *>(arg);
    return pNlApi->genlHandleMsg(msg, arg);
}
static int parser(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info, void *arg) {
    bool *complete = reinterpret_cast<bool *>(arg);
    *complete = true;
    return NL_OK;
}
}
TEST_F(SysmanNlApiFixture, GivenValidNlSockWhenCallingNlSocketModifyCbVerifyNlSocketModifyCbReturnsSuccess) {
    EXPECT_EQ(pNlApi->nlSocketModifyCb(pNlSock, NL_CB_VALID, NL_CB_CUSTOM, callback, reinterpret_cast<void *>(pNlApi)), 0);
}
TEST_F(SysmanNlApiFixture, GivenValidNlMsgWhenCallingNlaPutXVerifyNlaPutXReturnsSuccess) {
    struct nl_msg *pNlMsg = pNlApi->nlmsgAlloc();
    EXPECT_NE(pNlMsg, nullptr);
    EXPECT_NE(pNlApi->genlmsgPut(pNlMsg, NL_AUTO_PID, NL_AUTO_SEQ, SysmanNlApiFixture::testFamilyId, 0, 0, SysmanNlApiFixture::testCmd, 1), nullptr);
    EXPECT_EQ(pNlApi->nlaPutU16(pNlMsg, SysmanNlApiFixture::testAttrU16, 0x7fffU), 0);
    EXPECT_EQ(pNlApi->nlaPutU32(pNlMsg, SysmanNlApiFixture::testAttrU32, 0x7fffffffU), 0);
    EXPECT_EQ(pNlApi->nlaPutU64(pNlMsg, SysmanNlApiFixture::testAttrU64, 0x7fffffffffffffffUL), 0);
    EXPECT_EQ(pNlApi->nlaPutU8(pNlMsg, SysmanNlApiFixture::testAttrU8, 0x7fU), 0);
    pNlApi->nlmsgFree(pNlMsg);
}
TEST_F(SysmanNlApiFixture, GivenValidNlattrWhenCallingNlaGetXVerifyNlaGetXReturnsSuccess) {
    uint8_t buffer[NLA_HDRLEN + sizeof(uint64_t)];
    struct nlattr *attr = reinterpret_cast<struct nlattr *>(&buffer[0]);

    attr->nla_type = SysmanNlApiFixture::testAttrU8;
    attr->nla_len = NLA_HDRLEN + sizeof(uint8_t);
    *reinterpret_cast<uint8_t *>(&buffer[NLA_HDRLEN]) = 0x7fU;
    EXPECT_EQ(pNlApi->nlaGetU8(attr), *reinterpret_cast<uint8_t *>(&buffer[NLA_HDRLEN]));
    attr->nla_type = SysmanNlApiFixture::testAttrU32;
    attr->nla_len = NLA_HDRLEN + sizeof(uint32_t);
    *reinterpret_cast<uint32_t *>(&buffer[NLA_HDRLEN]) = 0x7fffffffU;
    EXPECT_EQ(pNlApi->nlaGetU32(attr), *reinterpret_cast<uint32_t *>(&buffer[NLA_HDRLEN]));
    attr->nla_type = SysmanNlApiFixture::testAttrU64;
    attr->nla_len = NLA_HDRLEN + sizeof(uint64_t);
    *reinterpret_cast<uint64_t *>(&buffer[NLA_HDRLEN]) = 0x7fffffffffffffffUL;
    EXPECT_EQ(pNlApi->nlaGetU64(attr), *reinterpret_cast<uint64_t *>(&buffer[NLA_HDRLEN]));
}
TEST_F(SysmanNlApiFixture, GivenValidNlattrWhenCallingNlaRoutinesVerifyNlaRoutinesReturnsSuccess) {
    uint8_t buffer[NLA_HDRLEN + sizeof(uint64_t)];
    struct nlattr *attr = reinterpret_cast<struct nlattr *>(&buffer[0]);
    int length = sizeof(uint8_t);
    attr->nla_type = SysmanNlApiFixture::testAttrU8;
    attr->nla_len = NLA_HDRLEN + length;
    *reinterpret_cast<uint8_t *>(&buffer[NLA_HDRLEN]) = 0x7fU;

    EXPECT_EQ(pNlApi->nlaData(attr), &buffer[NLA_HDRLEN]);
    EXPECT_EQ(pNlApi->nlaType(attr), SysmanNlApiFixture::testAttrU8);
    EXPECT_FALSE(pNlApi->nlaIsNested(attr));
    EXPECT_EQ(pNlApi->nlaLen(attr), length);
    int remainder = NLA_ALIGN(attr->nla_len);
    struct nlattr *next = nullptr;
    EXPECT_TRUE(pNlApi->nlaOk(attr, remainder));
    next = pNlApi->nlaNext(attr, &remainder);
    EXPECT_NE(next, nullptr);
    EXPECT_EQ(remainder, 0);
    EXPECT_FALSE(pNlApi->nlaOk(next, remainder));
}
TEST_F(SysmanNlApiFixture, GivenValidNlMsghdrWhenCallingNlmsgAttrdataVerifyNmsgAttrdataReturnsSuccess) {
    struct nl_msg *pNlMsg = pNlApi->nlmsgAlloc();
    EXPECT_NE(pNlMsg, nullptr);
    pNlApi->genlmsgPut(pNlMsg, NL_AUTO_PID, NL_AUTO_SEQ, SysmanNlApiFixture::testFamilyId, 0, 0, SysmanNlApiFixture::testCmd, 1);
    EXPECT_EQ(pNlApi->nlaPutU32(pNlMsg, SysmanNlApiFixture::testAttrU32, 0x7fffffffU), 0);
    EXPECT_EQ(pNlApi->nlaPutU64(pNlMsg, SysmanNlApiFixture::testAttrU64, 0x7fffffffffffffffUL), 0);
    EXPECT_EQ(pNlApi->nlaPutU8(pNlMsg, SysmanNlApiFixture::testAttrU8, 0x7fU), 0);

    struct nlmsghdr *hdr = pNlApi->nlmsgHdr(pNlMsg);
    struct nlattr *attr = nullptr;
    attr = pNlApi->nlmsgAttrdata(hdr, GENL_HDRLEN);
    int remainder = pNlApi->nlmsgAttrlen(hdr, GENL_HDRLEN);
    EXPECT_NE(attr, nullptr);
    EXPECT_TRUE(pNlApi->nlaOk(attr, remainder));
    void *data = (struct nlattr *)pNlApi->nlaData(attr);
    size_t len = pNlApi->nlaLen(attr);
    EXPECT_EQ(pNlApi->nlaType(attr), SysmanNlApiFixture::testAttrU32);
    EXPECT_EQ(len, sizeof(uint32_t));
    EXPECT_EQ(*reinterpret_cast<uint32_t *>(data), 0x7fffffffU);
    attr = pNlApi->nlaNext(attr, &remainder);
    EXPECT_NE(attr, nullptr);
    EXPECT_TRUE(pNlApi->nlaOk(attr, remainder));
    data = (struct nlattr *)pNlApi->nlaData(attr);
    len = pNlApi->nlaLen(attr);
    EXPECT_EQ(pNlApi->nlaType(attr), SysmanNlApiFixture::testAttrU64);
    EXPECT_EQ(len, sizeof(uint64_t));
    EXPECT_EQ(*reinterpret_cast<uint64_t *>(data), 0x7fffffffffffffffUL);
    attr = pNlApi->nlaNext(attr, &remainder);
    EXPECT_NE(attr, nullptr);
    EXPECT_TRUE(pNlApi->nlaOk(attr, remainder));
    data = (struct nlattr *)pNlApi->nlaData(attr);
    len = pNlApi->nlaLen(attr);
    EXPECT_EQ(pNlApi->nlaType(attr), SysmanNlApiFixture::testAttrU8);
    EXPECT_EQ(len, sizeof(uint8_t));
    EXPECT_EQ(*reinterpret_cast<uint8_t *>(data), 0x7fU);

    pNlApi->nlmsgFree(pNlMsg);
}
TEST_F(SysmanNlApiFixture, GivenValidNlMsghWhenCallingGenlHandleMsgVerifyGenlHandleMsgCallsMsgParserAndReturnsSuccess) {
    struct genl_ops ops = {};
    struct genl_cmd cmd = {};
    struct nla_policy policy[SysmanNlApiFixture::testAttrMax + 1] = {};
    policy[SysmanNlApiFixture::testAttrU8].type = NLA_U8;
    policy[SysmanNlApiFixture::testAttrU16].type = NLA_U16;
    policy[SysmanNlApiFixture::testAttrU32].type = NLA_U32;
    policy[SysmanNlApiFixture::testAttrU64].type = NLA_U64;

    cmd.c_id = SysmanNlApiFixture::testCmd;
    cmd.c_name = new char[strlen(SysmanNlApiFixture::testCmdName) + 1];
    strcpy(cmd.c_name, SysmanNlApiFixture::testCmdName);
    cmd.c_maxattr = SysmanNlApiFixture::testAttrMax;
    cmd.c_attr_policy = policy;
    cmd.c_msg_parser = &parser;

    ops.o_name = new char[strlen(SysmanNlApiFixture::testName) + 1];
    strcpy(ops.o_name, SysmanNlApiFixture::testName);
    ops.o_id = SysmanNlApiFixture::testFamilyId;
    ops.o_hdrsize = 0;
    ops.o_cmds = &cmd;
    ops.o_ncmds = 1;

    EXPECT_EQ(pNlApi->genlRegisterFamily(&ops), 0);

    struct nl_msg *pNlMsg = pNlApi->nlmsgAlloc();
    EXPECT_NE(pNlMsg, nullptr);
    EXPECT_NE(pNlApi->genlmsgPut(pNlMsg, NL_AUTO_PID, NL_AUTO_SEQ, SysmanNlApiFixture::testFamilyId, 0, 0, SysmanNlApiFixture::testCmd, 1), nullptr);
    bool complete = false;
    EXPECT_EQ(pNlApi->genlHandleMsg(pNlMsg, reinterpret_cast<void *>(&complete)), NL_OK);
    EXPECT_TRUE(complete);

    pNlApi->nlmsgFree(pNlMsg);
    EXPECT_EQ(pNlApi->genlUnregisterFamily(&ops), 0);
    delete ops.o_name;
    delete cmd.c_name;
}
TEST_F(SysmanNlApiFixture, GivenUnconnectedSocketWhenCallingRecvmsgsDefaultVerifyRecvmsgsDefaultReturnsFailure) {

    // there is no netlink server to talk to, just verify that linkage to nl_recvmsgs_default is correct
    EXPECT_LT(pNlApi->nlRecvmsgsDefault(pNlSock), 0);
}

TEST_F(SysmanNlApiFixture, GivenUnconnectedSocketWhenCallingNlSendAutoVerifyNlSendAutoReturnsFailure) {
    struct nl_msg *pNlMsg = pNlApi->nlmsgAlloc();
    EXPECT_NE(pNlMsg, nullptr);
    EXPECT_NE(pNlApi->genlmsgPut(pNlMsg, NL_AUTO_PID, NL_AUTO_SEQ, SysmanNlApiFixture::testFamilyId, 0, 0, SysmanNlApiFixture::testCmd, 1), nullptr);

    // there is no netlink server to talk to, just verify that linkage to nl_send_auto is correct
    EXPECT_LT(pNlApi->nlSendAuto(pNlSock, pNlMsg), 0);
    pNlApi->nlmsgFree(pNlMsg);
}

} // namespace ult
} // namespace L0
