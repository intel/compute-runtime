/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_nl_dll.h"

namespace L0 {
namespace ult {

extern "C" {
static int mockCallback(struct nl_msg *msg, void *arg) {
    return NL_OK;
}
}

struct nl_sock MockNlDll::mockNlSock;
struct nl_msg MockNlDll::mockNlMsg;
struct nlmsghdr MockNlDll::mockNlmsghdr;
struct nlattr MockNlDll::mockNlattr;
struct nlattr MockNlDll::mockNextNlattr;
struct genl_ops MockNlDll::mockGenlOps;
nl_recvmsg_msg_cb_t MockNlDll::mockCb = mockCallback;

extern "C" {
int mockGenlConnect(struct nl_sock *sock) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    return 0;
}

int mockGenlCtrlResolve(struct nl_sock *sock, const char *name) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    EXPECT_FALSE(strcmp(MockNlDll::mockFamilyName, name));
    return 0;
}

int mockGenlHandleMsg(struct nl_msg *msg, void *arg) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    EXPECT_EQ(MockNlDll::mockArgP, arg);
    return 0;
}

void *mockGenlmsgPut(struct nl_msg *msg, uint32_t port, uint32_t seq, int family, int hdrlen, int flags, uint8_t cmd, uint8_t version) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    EXPECT_EQ(MockNlDll::mockFamilyId, family);
    EXPECT_EQ(MockNlDll::mockPort, port);
    EXPECT_EQ(MockNlDll::mockSeq, seq);
    EXPECT_EQ(MockNlDll::mockCmd, cmd);
    EXPECT_EQ(MockNlDll::mockHdrlen, hdrlen);
    EXPECT_EQ(MockNlDll::mockFlags, flags);
    EXPECT_EQ(MockNlDll::mockVersion, version);
    return msg;
}

int mockGenlOpsResolve(struct nl_sock *sock, struct genl_ops *ops) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    EXPECT_EQ(&MockNlDll::mockGenlOps, ops);
    return 0;
}

int mockGenlRegisterFamily(struct genl_ops *ops) {
    EXPECT_EQ(&MockNlDll::mockGenlOps, ops);
    return 0;
}

int mockGenlUnregisterFamily(struct genl_ops *ops) {
    EXPECT_EQ(&MockNlDll::mockGenlOps, ops);
    return 0;
}

struct nl_sock *mockNlSocketAlloc() {
    return &MockNlDll::mockNlSock;
}

void mockNlSocketDisableSeqCheck(struct nl_sock *sock) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    return;
}

void mockNlSocketFree(struct nl_sock *sock) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    return;
}

int mockNlSocketModifyCb(struct nl_sock *sock, enum nl_cb_type type, enum nl_cb_kind kind, nl_recvmsg_msg_cb_t cb, void *arg) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    EXPECT_EQ(MockNlDll::mockCbType, type);
    EXPECT_EQ(MockNlDll::mockCbKind, kind);
    EXPECT_EQ(MockNlDll::mockCb, cb);
    EXPECT_EQ(MockNlDll::mockArgP, arg);
    return 0;
}

int mockNlRecvmsgsDefault(struct nl_sock *sock) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    return 0;
}

int mockNlSendAuto(struct nl_sock *sock, struct nl_msg *msg) {
    EXPECT_EQ(&MockNlDll::mockNlSock, sock);
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    return 0;
}

void *mockNlaData(const struct nlattr *attr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    return &MockNlDll::mockNlattr;
}

uint32_t mockNlaGetU32(const struct nlattr *attr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    return MockNlDll::mockU32Val;
}

uint64_t mockNlaGetU64(const struct nlattr *attr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    return MockNlDll::mockU64Val;
}

uint8_t mockNlaGetU8(const struct nlattr *attr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    return MockNlDll::mockU8Val;
}

int mockNlaIsNested(const struct nlattr *attr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    return 0;
}

int mockNlaLen(const struct nlattr *attr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    return MockNlDll::mockAttrLen;
}

struct nlattr *mockNlaNext(const struct nlattr *attr, int *remaining) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    EXPECT_EQ(MockNlDll::mockRemainBefore, *remaining);
    *remaining = MockNlDll::mockRemainAfter;
    return &MockNlDll::mockNextNlattr;
}

int mockNlaOk(const struct nlattr *attr, int remaining) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    EXPECT_EQ(MockNlDll::mockRemainBefore, remaining);
    return 0;
}

int mockNlaPutU16(struct nl_msg *msg, int type, uint16_t data) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    EXPECT_EQ(MockNlDll::mockType, type);
    EXPECT_EQ(MockNlDll::mockU16Val, data);
    return 0;
}

int mockNlaPutU32(struct nl_msg *msg, int type, uint32_t data) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    EXPECT_EQ(MockNlDll::mockType, type);
    EXPECT_EQ(MockNlDll::mockU32Val, data);
    return 0;
}

int mockNlaPutU64(struct nl_msg *msg, int type, uint64_t data) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    EXPECT_EQ(MockNlDll::mockType, type);
    EXPECT_EQ(MockNlDll::mockU64Val, data);
    return 0;
}

int mockNlaPutU8(struct nl_msg *msg, int type, uint8_t data) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    EXPECT_EQ(MockNlDll::mockType, type);
    EXPECT_EQ(MockNlDll::mockU8Val, data);
    return 0;
}

int mockNlaType(const struct nlattr *attr) {
    EXPECT_EQ(&MockNlDll::mockNlattr, attr);
    return MockNlDll::mockType;
}

struct nl_msg *mockNlmsgAlloc() {
    return &MockNlDll::mockNlMsg;
}

void mockNlmsgFree(struct nl_msg *msg) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    return;
}

struct nlattr *mockNlmsgAttrdata(const struct nlmsghdr *hdr, int attr) {
    EXPECT_EQ(&MockNlDll::mockNlmsghdr, hdr);
    EXPECT_EQ(MockNlDll::mockAttr, attr);
    return &MockNlDll::mockNlattr;
}

int mockNlmsgAttrlen(const struct nlmsghdr *hdr, int attr) {
    EXPECT_EQ(&MockNlDll::mockNlmsghdr, hdr);
    EXPECT_EQ(MockNlDll::mockAttr, attr);
    return MockNlDll::mockAttrLen;
}

struct nlmsghdr *mockNlmsgHdr(struct nl_msg *msg) {
    EXPECT_EQ(&MockNlDll::mockNlMsg, msg);
    return &MockNlDll::mockNlmsghdr;
}
}

MockNlDll::MockNlDll() {
    funcMap["genl_connect"] = reinterpret_cast<void *>(&mockGenlConnect);
    funcMap["genl_ctrl_resolve"] = reinterpret_cast<void *>(&mockGenlCtrlResolve);
    funcMap["genl_handle_msg"] = reinterpret_cast<void *>(&mockGenlHandleMsg);
    funcMap["genlmsg_put"] = reinterpret_cast<void *>(&mockGenlmsgPut);
    funcMap["genl_ops_resolve"] = reinterpret_cast<void *>(&mockGenlOpsResolve);
    funcMap["genl_register_family"] = reinterpret_cast<void *>(&mockGenlRegisterFamily);
    funcMap["genl_unregister_family"] = reinterpret_cast<void *>(&mockGenlUnregisterFamily);
    funcMap["nl_recvmsgs_default"] = reinterpret_cast<void *>(&mockNlRecvmsgsDefault);
    funcMap["nl_send_auto"] = reinterpret_cast<void *>(&mockNlSendAuto);
    funcMap["nl_socket_alloc"] = reinterpret_cast<void *>(&mockNlSocketAlloc);
    funcMap["nl_socket_disable_seq_check"] = reinterpret_cast<void *>(&mockNlSocketDisableSeqCheck);
    funcMap["nl_socket_free"] = reinterpret_cast<void *>(&mockNlSocketFree);
    funcMap["nl_socket_modify_cb"] = reinterpret_cast<void *>(&mockNlSocketModifyCb);
    funcMap["nla_data"] = reinterpret_cast<void *>(&mockNlaData);
    funcMap["nla_get_u32"] = reinterpret_cast<void *>(&mockNlaGetU32);
    funcMap["nla_get_u64"] = reinterpret_cast<void *>(&mockNlaGetU64);
    funcMap["nla_get_u8"] = reinterpret_cast<void *>(&mockNlaGetU8);
    funcMap["nla_is_nested"] = reinterpret_cast<void *>(&mockNlaIsNested);
    funcMap["nla_len"] = reinterpret_cast<void *>(&mockNlaLen);
    funcMap["nla_next"] = reinterpret_cast<void *>(&mockNlaNext);
    funcMap["nla_ok"] = reinterpret_cast<void *>(&mockNlaOk);
    funcMap["nla_put_u16"] = reinterpret_cast<void *>(&mockNlaPutU16);
    funcMap["nla_put_u32"] = reinterpret_cast<void *>(&mockNlaPutU32);
    funcMap["nla_put_u64"] = reinterpret_cast<void *>(&mockNlaPutU64);
    funcMap["nla_put_u8"] = reinterpret_cast<void *>(&mockNlaPutU8);
    funcMap["nla_type"] = reinterpret_cast<void *>(&mockNlaType);
    funcMap["nlmsg_alloc"] = reinterpret_cast<void *>(&mockNlmsgAlloc);
    funcMap["nlmsg_attrdata"] = reinterpret_cast<void *>(&mockNlmsgAttrdata);
    funcMap["nlmsg_attrlen"] = reinterpret_cast<void *>(&mockNlmsgAttrlen);
    funcMap["nlmsg_free"] = reinterpret_cast<void *>(&mockNlmsgFree);
    funcMap["nlmsg_hdr"] = reinterpret_cast<void *>(&mockNlmsgHdr);
}

void *MockNlDll::getProcAddress(const std::string &procName) {
    auto it = funcMap.find(procName);
    if (funcMap.end() == it) {
        return nullptr;
    } else {
        return it->second;
    }
}

void MockNlDll::deleteEntryPoint(const std::string &procName) {
    auto it = funcMap.find(procName);
    if (funcMap.end() != it) {
        funcMap.erase(it);
    }
}

} // namespace ult
} // namespace L0
