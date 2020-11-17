/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "nl_api.h"

namespace L0 {

static const std::string libgenlFile = "libnl-genl-3.so.200";
static const std::string genlConnectRoutine = "genl_connect";
static const std::string genlCtrlResolveRoutine = "genl_ctrl_resolve";
static const std::string genlHandleMsgRoutine = "genl_handle_msg";
static const std::string genlmsgPutRoutine = "genlmsg_put";
static const std::string genlOpsResolveRoutine = "genl_ops_resolve";
static const std::string genlRegisterFamilyRoutine = "genl_register_family";
static const std::string genlUnregisterFamilyRoutine = "genl_unregister_family";
static const std::string nlaDataRoutine = "nla_data";
static const std::string nlaGetU32Routine = "nla_get_u32";
static const std::string nlaGetU64Routine = "nla_get_u64";
static const std::string nlaGetU8Routine = "nla_get_u8";
static const std::string nlaIsNestedRoutine = "nla_is_nested";
static const std::string nlaLenRoutine = "nla_len";
static const std::string nlaNextRoutine = "nla_next";
static const std::string nlaOkRoutine = "nla_ok";
static const std::string nlaPutU16Routine = "nla_put_u16";
static const std::string nlaPutU32Routine = "nla_put_u32";
static const std::string nlaPutU64Routine = "nla_put_u64";
static const std::string nlaPutU8Routine = "nla_put_u8";
static const std::string nlaTypeRoutine = "nla_type";
static const std::string nlmsgAllocRoutine = "nlmsg_alloc";
static const std::string nlmsgHdrRoutine = "nlmsg_hdr";
static const std::string nlmsgAttrdataRoutine = "nlmsg_attrdata";
static const std::string nlmsgAttrlenRoutine = "nlmsg_attrlen";
static const std::string nlmsgFreeRoutine = "nlmsg_free";
static const std::string nlRecvmsgsDefaultRoutine = "nl_recvmsgs_default";
static const std::string nlSendAutoRoutine = "nl_send_auto";
static const std::string nlSocketAllocRoutine = "nl_socket_alloc";
static const std::string nlSocketDisableSeqCheckRoutine = "nl_socket_disable_seq_check";
static const std::string nlSocketFreeRoutine = "nl_socket_free";
static const std::string nlSocketModifyCbRoutine = "nl_socket_modify_cb";

template <class T>
bool NlApi::getProcAddr(NEO::OsLibrary *lh, const std::string name, T &proc) {
    proc = reinterpret_cast<T>(lh->getProcAddress(name));
    return nullptr != proc;
}

bool NlApi::loadEntryPoints() {
    bool ok = true;
    ok = ok && getProcAddr(genlLibraryHandle, genlConnectRoutine, genlConnectEntry);
    ok = ok && getProcAddr(genlLibraryHandle, genlCtrlResolveRoutine, genlCtrlResolveEntry);
    ok = ok && getProcAddr(genlLibraryHandle, genlHandleMsgRoutine, genlHandleMsgEntry);
    ok = ok && getProcAddr(genlLibraryHandle, genlmsgPutRoutine, genlmsgPutEntry);
    ok = ok && getProcAddr(genlLibraryHandle, genlOpsResolveRoutine, genlOpsResolveEntry);
    ok = ok && getProcAddr(genlLibraryHandle, genlRegisterFamilyRoutine, genlRegisterFamilyEntry);
    ok = ok && getProcAddr(genlLibraryHandle, genlUnregisterFamilyRoutine, genlUnregisterFamilyEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaDataRoutine, nlaDataEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaGetU32Routine, nlaGetU32Entry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaGetU64Routine, nlaGetU64Entry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaGetU8Routine, nlaGetU8Entry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaIsNestedRoutine, nlaIsNestedEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaLenRoutine, nlaLenEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaNextRoutine, nlaNextEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaOkRoutine, nlaOkEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaPutU16Routine, nlaPutU16Entry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaPutU32Routine, nlaPutU32Entry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaPutU64Routine, nlaPutU64Entry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaPutU8Routine, nlaPutU8Entry);
    ok = ok && getProcAddr(genlLibraryHandle, nlaTypeRoutine, nlaTypeEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlmsgAllocRoutine, nlmsgAllocEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlmsgHdrRoutine, nlmsgHdrEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlmsgAttrdataRoutine, nlmsgAttrdataEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlmsgAttrlenRoutine, nlmsgAttrlenEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlmsgFreeRoutine, nlmsgFreeEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlRecvmsgsDefaultRoutine, nlRecvmsgsDefaultEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlSendAutoRoutine, nlSendAutoEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlSocketAllocRoutine, nlSocketAllocEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlSocketDisableSeqCheckRoutine, nlSocketDisableSeqCheckEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlSocketFreeRoutine, nlSocketFreeEntry);
    ok = ok && getProcAddr(genlLibraryHandle, nlSocketModifyCbRoutine, nlSocketModifyCbEntry);

    return ok;
}

int NlApi::genlConnect(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == genlConnectEntry);
    return (*genlConnectEntry)(sock);
}

int NlApi::genlCtrlResolve(struct nl_sock *sock, const char *name) {
    UNRECOVERABLE_IF(nullptr == genlCtrlResolveEntry);
    return (*genlCtrlResolveEntry)(sock, name);
}

int NlApi::genlHandleMsg(struct nl_msg *msg, void *arg) {
    UNRECOVERABLE_IF(nullptr == genlHandleMsgEntry);
    return (*genlHandleMsgEntry)(msg, arg);
}

void *NlApi::genlmsgPut(struct nl_msg *msg, uint32_t port, uint32_t seq, int family, int hdrlen, int flags, uint8_t cmd, uint8_t version) {
    UNRECOVERABLE_IF(nullptr == genlmsgPutEntry);
    return (*genlmsgPutEntry)(msg, port, seq, family, hdrlen, flags, cmd, version);
}

int NlApi::genlOpsResolve(struct nl_sock *sock, struct genl_ops *ops) {
    UNRECOVERABLE_IF(nullptr == genlOpsResolveEntry);
    return (*genlOpsResolveEntry)(sock, ops);
}

int NlApi::genlRegisterFamily(struct genl_ops *ops) {
    UNRECOVERABLE_IF(nullptr == genlRegisterFamilyEntry);
    return (*genlRegisterFamilyEntry)(ops);
}

int NlApi::genlUnregisterFamily(struct genl_ops *ops) {
    UNRECOVERABLE_IF(nullptr == genlUnregisterFamilyEntry);
    return (*genlUnregisterFamilyEntry)(ops);
}

void *NlApi::nlaData(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaDataEntry);
    return (*nlaDataEntry)(attr);
}

uint32_t NlApi::nlaGetU32(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaGetU32Entry);
    return (*nlaGetU32Entry)(attr);
}

uint64_t NlApi::nlaGetU64(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaGetU64Entry);
    return (*nlaGetU64Entry)(attr);
}

uint8_t NlApi::nlaGetU8(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaGetU8Entry);
    return (*nlaGetU8Entry)(attr);
}

int NlApi::nlaIsNested(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaIsNestedEntry);
    return (*nlaIsNestedEntry)(attr);
}

int NlApi::nlaLen(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaLenEntry);
    return (*nlaLenEntry)(attr);
}

struct nlattr *NlApi::nlaNext(const struct nlattr *attr, int *remaining) {
    UNRECOVERABLE_IF(nullptr == nlaNextEntry);
    return (*nlaNextEntry)(attr, remaining);
}

int NlApi::nlaOk(const struct nlattr *attr, int remaining) {
    UNRECOVERABLE_IF(nullptr == nlaOkEntry);
    return (*nlaOkEntry)(attr, remaining);
}

int NlApi::nlaPutU16(struct nl_msg *msg, int id, uint16_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU16Entry);
    return (*nlaPutU16Entry)(msg, id, data);
}

int NlApi::nlaPutU32(struct nl_msg *msg, int id, uint32_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU32Entry);
    return (*nlaPutU32Entry)(msg, id, data);
}

int NlApi::nlaPutU64(struct nl_msg *msg, int id, uint64_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU64Entry);
    return (*nlaPutU64Entry)(msg, id, data);
}

int NlApi::nlaPutU8(struct nl_msg *msg, int id, uint8_t data) {
    UNRECOVERABLE_IF(nullptr == nlaPutU8Entry);
    return (*nlaPutU8Entry)(msg, id, data);
}

int NlApi::nlaType(const struct nlattr *attr) {
    UNRECOVERABLE_IF(nullptr == nlaTypeEntry);
    return (*nlaTypeEntry)(attr);
}

struct nl_msg *NlApi::nlmsgAlloc() {
    UNRECOVERABLE_IF(nullptr == nlmsgAllocEntry);
    return (*nlmsgAllocEntry)();
}

struct nlmsghdr *NlApi::nlmsgHdr(struct nl_msg *msg) {
    UNRECOVERABLE_IF(nullptr == nlmsgHdrEntry);
    return (*nlmsgHdrEntry)(msg);
}

struct nlattr *NlApi::nlmsgAttrdata(const struct nlmsghdr *hdr, int attr) {
    UNRECOVERABLE_IF(nullptr == nlmsgAttrdataEntry);
    return (*nlmsgAttrdataEntry)(hdr, attr);
}

int NlApi::nlmsgAttrlen(const struct nlmsghdr *hdr, int attr) {
    UNRECOVERABLE_IF(nullptr == nlmsgAttrlenEntry);
    return (*nlmsgAttrlenEntry)(hdr, attr);
}

void NlApi::nlmsgFree(struct nl_msg *msg) {
    UNRECOVERABLE_IF(nullptr == nlmsgFreeEntry);
    (*nlmsgFreeEntry)(msg);
    return;
}

int NlApi::nlRecvmsgsDefault(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == nlRecvmsgsDefaultEntry);
    return (*nlRecvmsgsDefaultEntry)(sock);
}

int NlApi::nlSendAuto(struct nl_sock *sock, struct nl_msg *msg) {
    UNRECOVERABLE_IF(nullptr == nlSendAutoEntry);
    return (*nlSendAutoEntry)(sock, msg);
}

struct nl_sock *NlApi::nlSocketAlloc() {
    UNRECOVERABLE_IF(nullptr == nlSocketAllocEntry);
    return (*nlSocketAllocEntry)();
}

void NlApi::nlSocketDisableSeqCheck(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == nlSocketDisableSeqCheckEntry);
    (*nlSocketDisableSeqCheckEntry)(sock);
    return;
}

void NlApi::nlSocketFree(struct nl_sock *sock) {
    UNRECOVERABLE_IF(nullptr == nlSocketFreeEntry);
    (*nlSocketFreeEntry)(sock);
    return;
}

int NlApi::nlSocketModifyCb(struct nl_sock *sock, enum nl_cb_type type, enum nl_cb_kind kind, nl_recvmsg_msg_cb_t cb, void *arg) {
    UNRECOVERABLE_IF(nullptr == nlSocketModifyCbEntry);
    return (*nlSocketModifyCbEntry)(sock, type, kind, cb, arg);
}

NlApi *NlApi::create() {
    NlApi *pNlApi = new NlApi;
    UNRECOVERABLE_IF(nullptr == pNlApi);

    pNlApi->genlLibraryHandle = NEO::OsLibrary::load(libgenlFile);
    if (nullptr == pNlApi->genlLibraryHandle) {
        delete pNlApi;
        return nullptr;
    }
    if (!pNlApi->loadEntryPoints()) {
        delete pNlApi;
        return nullptr;
    }
    return pNlApi;
}

NlApi::~NlApi() {
    if (nullptr != genlLibraryHandle) {
        delete genlLibraryHandle;
        genlLibraryHandle = nullptr;
    }
}
} // namespace L0
